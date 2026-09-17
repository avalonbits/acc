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
 * Backwards-linear rather than hashed on purpose. A function has a handful of
 * locals and the walk stops at the first match, so the hash would cost more in
 * table than it saved in comparisons. If a program ever has enough file-scope
 * functions for the tail of the walk to matter, that tail is the part to
 * index -- it is the part that does not change.
 *
 * Symbols are named by index and not by address. The array is grown with
 * realloc, so every Sym * in the program is invalidated by the next push, and
 * the two places that most want to hold on to a symbol -- a call waiting for
 * its callee's address, and a function definition waiting to record its own --
 * are both separated from their push by an arbitrary amount of parsing. Handing
 * out indices makes that safe by construction rather than by remembering. */
static Sym    *syms;
static int     nsyms, nglobals, cap;

void sym_init(void)
{
    cap = 64;
    syms = malloc(cap * sizeof *syms);
    if (!syms)
        acc_error("out of memory for symbols");
    nsyms = nglobals = 0;
}

int sym_push(NameRef name, int kind, int val)
{
    Sym *s;

    if (nsyms == cap) {
        cap *= 2;
        syms = realloc(syms, cap * sizeof *syms);
        if (!syms)
            acc_error("out of memory for symbols");
    }
    if (kind == SYM_LOCAL) {
        s = &syms[nsyms];
        s->name = name;
        s->kind = (unsigned char) kind;
        s->val = val;

        return nsyms++;
    }

    /* File scope. It goes in below the locals, which keeps every index
     * already handed out for a file-scope symbol -- the ones the code
     * generator is holding in its fixups -- pointing at the same symbol.
     * Only the locals move, and nothing holds a local's index across a push.
     * A function has a handful of them, so the move is a few dozen bytes. */
    memmove(&syms[nglobals + 1], &syms[nglobals],
            (size_t) (nsyms - nglobals) * sizeof *syms);
    s = &syms[nglobals];
    s->name = name;
    s->kind = (unsigned char) kind;
    s->val = val;
    nsyms++;

    return nglobals++;
}

int sym_find(NameRef name)
{
    /* The counter is unsigned so the loop test is not a signed compare. Two
     * signed ints compared with `<` cannot be done in one subtract on this
     * target, so the compiler adds `call pe, __setflag` to repair the flags
     * on overflow -- in a walk that runs once per name the parser sees and is
     * as long as the program has functions. A count cannot be negative, so
     * saying so in the type costs nothing. */
    unsigned i = (unsigned) nsyms;

    while (i-- > 0)
        if (syms[i].name == name)
            return (int) i;

    return SYM_NONE;
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
