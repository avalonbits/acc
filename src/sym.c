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
 * The locals are walked backwards and the file-scope names are looked up
 * directly, which is the split the shape of the problem asks for. A function
 * has a handful of locals and the walk stops at the first match, so a table
 * would cost more than it saved. File scope is the other way round: it only
 * grows, and a walk of it is as long as the program has functions. Each name
 * carries its file-scope symbol with it in the name arena (name_global), so
 * finding one is a load.
 *
 * Measured, when file scope was walked too: two inputs identical byte for byte
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
 * out indices makes that safe by construction rather than by remembering.
 *
 * An index is the symbol's offset into the table in bytes, not its position.
 * Turning a position into an address is a multiply by the size of a Sym, which
 * on this target is `call __ishl` -- there is no barrel shifter -- and it was
 * paid wherever a symbol was touched: several times a name in the parser, and
 * in every step of the walk below. An offset needs only the add. So the
 * counts below are in bytes too, and step by the size of a Sym. */
Sym           *sym_table;     /* sym_at reads it directly */
static int     nsyms, nglobals, cap;    /* bytes, not symbols */

#ifdef ACC_HASH_STATS
unsigned long sym_probes;
#define COUNT_PROBE() (sym_probes++)
#else
#define COUNT_PROBE() ((void) 0)
#endif

void sym_init(void)
{
    cap = 64 * sizeof *sym_table;
    sym_table = malloc(cap);
    if (!sym_table)
        acc_error("out of memory for symbols");
    nsyms = nglobals = 0;
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
    sym_table = realloc(sym_table, cap);
    if (!sym_table)
        acc_error("out of memory for symbols");
}

int sym_push(NameRef name, int kind, int val)
{
    Sym *sym;
    int at;

    if (nsyms == cap)
        syms_grow();
    if (sym_kind_local(kind)) {
        at = nsyms;
        sym = sym_at(at);
        sym->name = name;
        sym->kind = (unsigned char) kind;
        sym->val = val;
        sym->type = TY_INT;     /* until the declaration says otherwise */
        nsyms += sizeof *sym;

        return at;
    }

    /* File scope. It goes in below the locals, which keeps every index
     * already handed out for a file-scope symbol -- the ones the code
     * generator is holding in its fixups -- pointing at the same symbol.
     * Only the locals move, and nothing holds a local's index across a push.
     * A function has a handful of them, so the move is a few dozen bytes. */
    at = nglobals;
    sym = sym_at(at);
    memmove(sym + 1, sym, (size_t) (nsyms - at));
    sym->name = name;
    sym->kind = (unsigned char) kind;
    sym->val = val;
    sym->type = TY_INT;         /* a function called before it is defined is
                                 * assumed to return int, as C says */
    sym_set_params(at, 0, 0);   /* and to take nothing known */
    nsyms += sizeof *sym;
    nglobals += sizeof *sym;
    name_set_global(name, at);

    return at;
}

int sym_find(NameRef name)
{
    /* The locals, innermost first, so one shadows a file-scope name of the
     * same spelling. There are a handful and the walk stops at the first
     * match. The counter is unsigned so the loop test is not a signed
     * compare, which on this target is a helper call to repair the flags. */
    unsigned i = (unsigned) nsyms;

    while (i > (unsigned) nglobals) {
        i -= sizeof(Sym);
        COUNT_PROBE();
        if (sym_at(i)->name == name)
            return (int) i;
    }

    COUNT_PROBE();

    return name_global(name);
}


/* The end of a function. Functions do not nest, so there is exactly one
 * scope to drop and no mark to remember: everything above the file-scope
 * region is this function's. */
/* ------------------------------------------------------------------ */
/* what a function takes                                               */

/* The declared type of every parameter of every function, end to end, with
 * each function's Sym recording where its run starts and how long it is.
 *
 * A call has to convert each argument to the type the parameter was declared
 * with -- a long takes four bytes in two slots where an int takes three in
 * one, so without this a call to a function with a long parameter puts the
 * bytes in the wrong places and everything after it reads the wrong slot.
 * C89 would call that the programmer's fault for not writing a prototype;
 * C99, which acc is aimed at, requires the conversion.
 *
 * Kept beside the symbols rather than in them: a Sym is eight bytes, and two
 * more fields would make it twelve for every local as well as every
 * function. */
static Type    *param_type;
static unsigned param_used, param_cap;

/* Where each function's run starts and how long it is, indexed by its symbol.
 * File-scope symbols keep their index when others are added, so this stays
 * lined up with them.
 *
 * A record is as wide as a Sym, so that a symbol's index -- its offset into
 * the symbol table -- is also its signature's offset into this one, and
 * finding it is an add like sym_at. */
typedef union {
    struct {
        int           first;
        unsigned char count;
    } s;
    unsigned char size[sizeof(Sym)];
} FnSig;

typedef char fn_sig_is_a_sym_wide[sizeof(FnSig) == sizeof(Sym) ? 1 : -1];

static unsigned char *fn_sigs;
static unsigned       fn_cap;           /* bytes */

#define fn_sig(sym)  ((FnSig *) (fn_sigs + (sym)))

static void fn_room(unsigned want)
{
    if (want < fn_cap)
        return;
    while (fn_cap <= want)
        fn_cap = fn_cap ? fn_cap * 2 : 64 * sizeof(FnSig);
    fn_sigs = realloc(fn_sigs, fn_cap);
    if (!fn_sigs)
        acc_error("out of memory for the function signatures");
}

void sym_set_params(int sym, int first, int count)
{
    fn_room((unsigned) sym);
    fn_sig(sym)->s.first = first;
    fn_sig(sym)->s.count = (unsigned char) count;
}

/* The reads need no room check: a symbol only has a signature because
 * sym_push or sym_set_params made room for it first, and both run before any
 * call site can ask. Checking on every read cost a compare and a branch on
 * the path of every argument of every call. */
int sym_params_first(int sym)
{
    return fn_sig(sym)->s.first;
}

int sym_nparams(int sym)
{
    return fn_sig(sym)->s.count;
}

int sym_params_begin(void)
{
    return (int) param_used;
}

void sym_param_add(Type type)
{
    if (param_used == param_cap) {
        param_cap = param_cap ? param_cap * 2 : 64;
        param_type = realloc(param_type, param_cap * sizeof *param_type);
        if (!param_type)
            acc_error("out of memory for the parameter types");
    }
    param_type[param_used++] = type;
}

Type sym_param_type(int first, int index)
{
    return param_type[first + index];
}

/* The mark is a count of bytes above the file-scope symbols rather than a
 * position in the table, because a file-scope symbol pushed inside the scope
 * -- a call to a function not seen yet -- goes in underneath the locals and
 * moves them all along by one. */
int sym_scope_begin(void)
{
    return nsyms - nglobals;
}

void sym_scope_end(int mark)
{
    nsyms = nglobals + mark;
}

void sym_drop_locals(void)
{
    nsyms = nglobals;
}
