/*
 * The runtime's routines: calls to them, and the names the link finds them
 * by in the library.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "gen_int.h"

/* ------------------------------------------------------------------ */
/* the runtime                                                         */

/* The operations the chip has no instruction for, and every function's
 * prologue, are routines in libc.a: see lib/rt/. A call to one is a call to
 * a function the program does not define, which the link takes from the
 * library like any other -- and a program that uses none carries none.
 *
 * They are called by the thousand, so a call is not a fixup against a
 * symbol as it is made: pushing a file-scope symbol while a function is
 * being compiled would move its locals. It is written down as which routine
 * and where, and named once the compile is done (rt_name_all), when the
 * link can look for the names and gen_finish fill the slots in. */
RtFixup *rt_fixups;
int      nrt_fixups, rt_fixups_cap;

/* What each is called, without the acc_rt_ in front: in the order of the
 * enum in runtime.h. */
static const char *const rt_names[RT_COUNT] = {
    "frameset", "frameset0",
    "memcpy", "memmove", "memset", "memchr",
    "and", "or", "xor", "shl", "shru", "shrs",
    "mul", "divu", "remu", "divs", "rems",
    "ladd", "lsub", "land", "lor", "lxor", "lcmpeq", "lcmpord",
    "lshl", "lshru", "lshrs", "lmul", "ldivu", "lremu", "ldivs",
    "lrems", "lneg", "lnot",
    "itof", "uitof", "ftoi", "fcmp", "fsub", "fadd", "fmul", "fdiv",
    "ltof", "ultof", "ftol",
    "lladd", "llsub", "lland", "llor", "llxor", "llcmpeq",
    "llcmpord", "llneg", "llnot", "llshl", "llshru", "llshrs",
    "llmul", "lldivu", "llremu", "lldivs", "llrems",
    "lltof", "ulltof", "ftoll",
};

/* The symbol each routine is known by, once something has named it. */
static int rt_syms[RT_COUNT];

void rt_syms_init(void)
{
    int i;

    for (i = 0; i != RT_COUNT; i++)
        rt_syms[i] = SYM_NONE;
}

/* Its symbol, made the first time one is asked for. Only once a compile is
 * done, where pushing a file-scope symbol cannot move a Sym * that something
 * is holding. */
int rt_symbol(int which)
{
    if (rt_syms[which] == SYM_NONE) {
        char name[32];
        int sym, len;

        len = snprintf(name, sizeof name, "acc_rt_%s", rt_names[which]);
        sym = sym_push(name_intern(name, len), SYM_FUNC, 0);
        sym_set_flags(sym, SYMF_DECLARED | SYMF_PARAMS);
        rt_syms[which] = sym;
    }

    return rt_syms[which];
}

/* The operators with no instruction behind them. */
int needs_helper(int op)
{
    switch (op) {
    case TK_AMP: case TK_PIPE: case TK_CARET:
    case TK_SHL: case TK_SHR:
    case TK_STAR: case TK_SLASH: case TK_PERCENT:
        return 1;
    }

    return 0;
}

void rt_call(int which)
{
    if (nrt_fixups == rt_fixups_cap) {
        rt_fixups_cap = rt_fixups_cap ? rt_fixups_cap * 2 : 16;
        rt_fixups = realloc(rt_fixups, rt_fixups_cap * sizeof *rt_fixups);
        if (!rt_fixups)
            acc_error("out of memory for the runtime fixups");
    }
    out_opcode24(0xcd, 0);                       /* call nn */
    rt_fixups[nrt_fixups].which = (unsigned char) which;
    rt_fixups[nrt_fixups].at = out_here() - ACC_INT_SIZE;
    out_reloc(rt_fixups[nrt_fixups].at);
    nrt_fixups++;
}

/* Every routine the compile called, named: before the objects and
 * libraries are read, so that the link looks for them there. The file's
 * unused static functions go first, as they do from an object, so that
 * what only they called is not taken from the library. */
void rt_name_all(void)
{
    int i;

#ifndef ACC_NODROP
    drop_unused_statics();
#endif

    for (i = 0; i != nrt_fixups; i++)
        rt_symbol(rt_fixups[i].which);
}

/* The calls into the runtime, for the link: how many, where each is, and
 * what it calls -- which it asks of a library in the order of their slots,
 * among the other calls, as a link of this file's object would. */
int gen_nrt(void)
{
    return nrt_fixups;
}

int gen_rt_at(int i)
{
    return rt_fixups[i].at;
}

int gen_rt_sym(int i)
{
    return rt_syms[rt_fixups[i].which];
}

/* The slot of the k-th call, with the address of its routine to be added:
 * see out_add_later. The calls are in the order of their slots, as they
 * were written. */
static OutAdd rt_add;

static const OutAdd *rt_add_at(int k)
{
    rt_add.at = rt_fixups[k].at;
    rt_add.value = sym_at(rt_syms[rt_fixups[k].which])->val;

    return &rt_add;
}

/* A routine of the runtime that the link did not find: the library missing,
 * or a link without it. */
void rt_missing(int sym)
{
    acc_error("'%s' is acc's runtime and is in libc.a, which this link "
              "does not have", name_text(sym_at(sym)->name));
}

/* Every call filled in with where its routine went: in memory now, and in
 * the image's file as the sweep passes it. A routine the link did not find
 * is the library missing, or one without it. */
void rt_fill_all(void)
{
    int i, n = 0, last = -1;

    for (i = 0; i != nrt_fixups; i++) {
        int sym = rt_symbol(rt_fixups[i].which);

        if (!(sym_flags(sym) & SYMF_DEFINED))
            rt_missing(sym);
    }

    /* The ones in the file, in the order of their slots, from the first:
     * the sweep adds them as it passes. They are, the image being written
     * in order; one that is not is a patch of its own. */
    while (n != nrt_fixups
           && (unsigned) (rt_fixups[n].at - out_base) < (unsigned) out_flushed
           && rt_fixups[n].at > last)
        last = rt_fixups[n++].at;
    for (i = n; i != nrt_fixups; i++)
        out_add24(rt_fixups[i].at, sym_at(rt_syms[rt_fixups[i].which])->val);
    if (n)
        out_add_later2(rt_add_at, n);
}
