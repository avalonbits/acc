/*
 * The routines of acc's runtime that the code generator calls: see
 * lib/rt/, where they are, and runtime.c.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_RUNTIME_H
#define ACC_RUNTIME_H

/* In the order of rt_names in runtime.c, which says what each is called. */
enum {
    RT_FRAMESET, RT_FRAMESET0,
    RT_MEMCPY, RT_MEMMOVE, RT_MEMSET, RT_MEMCHR,
    RT_AND, RT_OR, RT_XOR, RT_SHL, RT_SHRU, RT_SHRS,
    RT_MUL, RT_DIVU, RT_REMU, RT_DIVS, RT_REMS,
    RT_LADD, RT_LSUB, RT_LAND, RT_LOR, RT_LXOR, RT_LCMPEQ, RT_LCMPORD,
    RT_LSHL, RT_LSHRU, RT_LSHRS, RT_LMUL, RT_LDIVU, RT_LREMU, RT_LDIVS,
    RT_LREMS, RT_LNEG, RT_LNOT,
    RT_ITOF, RT_UITOF, RT_FTOI, RT_FCMP, RT_FSUB, RT_FADD, RT_FMUL, RT_FDIV,
    RT_LTOF, RT_ULTOF, RT_FTOL,
    RT_LLADD, RT_LLSUB, RT_LLAND, RT_LLOR, RT_LLXOR, RT_LLCMPEQ,
    RT_LLCMPORD, RT_LLNEG, RT_LLNOT, RT_LLSHL, RT_LLSHRU, RT_LLSHRS,
    RT_LLMUL, RT_LLDIVU, RT_LLREMU, RT_LLDIVS, RT_LLREMS,
    RT_LLTOF, RT_ULLTOF, RT_FTOLL,
    RT_SHR, RT_SDIV,
    RT_COUNT
};

#endif
