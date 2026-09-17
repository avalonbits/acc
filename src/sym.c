/*
 * The symbol table.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 */

#include <stdlib.h>
#include <string.h>

#include "acc.h"

/* One array in two regions: file-scope symbols at the bottom, the current
 * function's parameters and locals above them, dropped when it ends. Lookup
 * walks backwards, so a local naturally shadows a file-scope name without any
 * scope chain to maintain.
 *
 * Which region a symbol goes in follows from its kind, so no caller can put
 * one in the wrong place. That matters because the tempting thing -- push
 * wherever the parser happens to be -- is wrong for the case that matters
 * most: a call to a function that has not been defined yet creates the
 * symbol, and the call is inside some other function, so pushing it as a
 * local would drop it at that function's closing brace and leave the call
 * pointing at nothing.
 *
 * The locals are walked backwards and the file-scope names are indexed, which
 * is the split the shape of the problem asks for. A function has a handful of
 * locals and the walk stops at the first match, so a table would cost more
 * than it saved. File scope is the other way round: it only grows, every call
 * searches all of it, and the search is as long as the program has functions.
 *
 * Measured, before the index existed: two inputs identical byte for byte
 * except which of two hundred functions main called -- the last one declared
 * or the first -- took 7.80 s and 8.76 s for ten compiles. Forty-four cycles
 * a symbol walked, on a walk whose length is the program's, done once per
 * name the program mentions. Quadratic, and this machine is not fast enough
 * to carry that.
 *
 * Symbols are named by index and not by address. The array is grown with
 * realloc, so every Sym * in the program is invalidated by the next push, and
 * the two places that most want to hold on to a symbol -- a call waiting for
 * its callee's address, and a function definition waiting to record its own --
 * are both separated from their push by an arbitrary amount of parsing. Handing
 * out indices makes that safe by construction rather than by remembering. */
static Sym    *syms;
static int     nsyms, nglobals, cap;

/* Name to file-scope symbol, open addressed, so that finding a function is a
 * hash and a compare rather than a walk. A name has at most one file-scope
 * symbol -- every push of one goes through a sym_find that came back empty --
 * which is what lets a plain map stand in for a search.
 *
 * Keyed on the NameRef, which is an offset into the name arena and so already
 * distinct per name; the low bits spread well enough because names are laid
 * down end to end and their lengths vary. No hashing of the text: the text
 * was hashed once when the name was interned, and this is the cheaper thing
 * to do with the result.
 *
 * Entries are never removed. File scope does not end. */
/* One entry per slot, eight bytes wide. Two things about that.
 *
 * It is one array rather than two columns because a probe reads both halves
 * of an entry, and two arrays mean two addresses to work out where one does.
 *
 * It is eight bytes rather than six because turning a slot number into an
 * address costs a helper call either way on this target, and the only choice
 * is which: six bytes is `call __imulu` and eight is `call __ishl`, which is
 * cheaper. The two spare bytes are per slot in a table that is twice the
 * number of functions in the program.
 *
 * An empty slot has NAME_NONE in name. */
typedef struct {
    NameRef       name;
    int           sym;
    unsigned char pad[2];
} SymSlot;

static SymSlot *index_table;
static unsigned index_cap, index_count;

#ifdef ACC_HASH_STATS
unsigned long sym_probes;
#define COUNT_PROBE() (sym_probes++)
#else
#define COUNT_PROBE() ((void) 0)
#endif

static void index_alloc(unsigned n)
{
    index_table = calloc(n, sizeof *index_table);
    if (!index_table)
        acc_error("out of memory for the symbol index");
    index_cap = n;
}

static void index_grow(void)
{
    SymSlot *old_table = index_table;
    unsigned old_cap = index_cap, i;

    index_alloc(old_cap * 2);
    for (i = 0; i < old_cap; i++) {
        unsigned slot;

        if (old_table[i].name == NAME_NONE)
            continue;
        slot = old_table[i].name & (index_cap - 1);
        while (index_table[slot].name != NAME_NONE)
            slot = (slot + 1) & (index_cap - 1);
        index_table[slot] = old_table[i];
    }
    free(old_table);
}

static void index_add(NameRef name, int sym)
{
    unsigned slot;

    /* Kept under half full. Past that a linear probe starts walking. */
    if ((index_count + 1) * 2 >= index_cap)
        index_grow();

    slot = name & (index_cap - 1);
    while (index_table[slot].name != NAME_NONE)
        slot = (slot + 1) & (index_cap - 1);
    index_table[slot].name = name;
    index_table[slot].sym = sym;
    index_count++;
}

static int index_find(NameRef name)
{
    unsigned slot = name & (index_cap - 1);

    COUNT_PROBE();
    while (index_table[slot].name != NAME_NONE) {
        if (index_table[slot].name == name)
            return index_table[slot].sym;
        slot = (slot + 1) & (index_cap - 1);
        COUNT_PROBE();
    }

    return SYM_NONE;
}

void sym_init(void)
{
    cap = 64;
    syms = malloc(cap * sizeof *syms);
    if (!syms)
        acc_error("out of memory for symbols");
    nsyms = nglobals = 0;
    index_alloc(128);
    index_count = 0;
}

/* Out of line for the same reason out_grow is: sym_push runs for every name
 * the program declares, and inlining a realloc and an error string into it
 * buys a stack frame on every one of them to serve a path taken a handful of
 * times in a compile. The attribute is load-bearing -- called from one place,
 * it goes straight back inline without it. */
__attribute__((noinline))
static void syms_grow(void)
{
    cap *= 2;
    syms = realloc(syms, cap * sizeof *syms);
    if (!syms)
        acc_error("out of memory for symbols");
}

int sym_push(NameRef name, int kind, int val)
{
    Sym *sym;

    if (nsyms == cap)
        syms_grow();
    if (kind == SYM_LOCAL) {
        sym = &syms[nsyms];
        sym->name = name;
        sym->kind = (unsigned char) kind;
        sym->val = val;
        sym->type = TY_INT;     /* until the declaration says otherwise */

        return nsyms++;
    }

    /* File scope. It goes in below the locals, which keeps every index
     * already handed out for a file-scope symbol -- the ones the code
     * generator is holding in its fixups -- pointing at the same symbol.
     * Only the locals move, and nothing holds a local's index across a push.
     * A function has a handful of them, so the move is a few dozen bytes. */
    memmove(&syms[nglobals + 1], &syms[nglobals],
            (size_t) (nsyms - nglobals) * sizeof *syms);
    sym = &syms[nglobals];
    sym->name = name;
    sym->kind = (unsigned char) kind;
    sym->val = val;
    sym->type = TY_INT;         /* a function called before it is defined is
                                 * assumed to return int, as C says */
    nsyms++;
    index_add(name, nglobals);

    return nglobals++;
}

int sym_find(NameRef name)
{
    /* The locals, innermost first, so one shadows a file-scope name of the
     * same spelling. There are a handful and the walk stops at the first
     * match. The counter is unsigned so the loop test is not a signed
     * compare, which on this target is a helper call to repair the flags. */
    unsigned i = (unsigned) nsyms;

    while (i-- > (unsigned) nglobals) {
        COUNT_PROBE();
        if (syms[i].name == name)
            return (int) i;
    }

    return index_find(name);
}

/* Good until the next sym_push and no longer. Callers fetch it where they use
 * it; nothing keeps one across a push. */
Sym *sym_at(int i)
{
    return &syms[i];
}

/* The end of a function. Functions do not nest, so there is exactly one
 * scope to drop and no mark to remember: everything above the file-scope
 * region is this function's. */
void sym_drop_locals(void)
{
    nsyms = nglobals;
}
