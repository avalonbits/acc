/*
 * The symbol table.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 */

#include <stdlib.h>

#include "acc.h"

/* One array used as a stack: file-scope symbols at the bottom, a function's
 * parameters and locals pushed above them and dropped when it ends. Lookup
 * walks backwards, so an inner name naturally shadows an outer one without
 * any scope chain to maintain.
 *
 * Backwards-linear rather than hashed on purpose. A function has a handful of
 * locals and the walk stops at the first match, so the hash would cost more in
 * table than it saved in comparisons. If a program ever has enough file-scope
 * functions for the tail of the walk to matter, that tail is the part to
 * index -- it is the part that does not change. */
static Sym    *syms;
static int     nsyms, cap;

void sym_init(void)
{
    cap = 64;
    syms = malloc(cap * sizeof *syms);
    if (!syms)
        acc_error("out of memory for symbols");
    nsyms = 0;
}

Sym *sym_push(NameRef name, int kind, int val)
{
    Sym *s;

    if (nsyms == cap) {
        cap *= 2;
        syms = realloc(syms, cap * sizeof *syms);
        if (!syms)
            acc_error("out of memory for symbols");
    }
    s = &syms[nsyms++];
    s->name = name;
    s->kind = (unsigned char) kind;
    s->val = val;

    return s;
}

Sym *sym_find(NameRef name)
{
    int i;

    for (i = nsyms - 1; i >= 0; i--)
        if (syms[i].name == name)
            return &syms[i];

    return NULL;
}

int sym_mark(void)
{
    return nsyms;
}

void sym_release(int mark)
{
    nsyms = mark;
}
