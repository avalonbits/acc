/*
 * opt-acc's log of the parser's calls into the code generator, as
 * genlog.c keeps it and ssa.c reads it.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_GENLOG_H
#define ACC_GENLOG_H

#ifdef OPT_ACC

/* One call: what it was, its arguments, what it answered, and what it left
 * behind -- where the image stood, how many fixups, runtime calls, bss
 * fixups and relocations there were, and the value stack: how deep it was
 * going in and coming out, and the value on top, as the classic backend
 * made it. */
typedef struct {
    unsigned short op;
    long arg[6];                /* the arguments, in order */
    long out[4];                /* what each pointer argument came back as */
    long ret;
    int  at_after;              /* out_here() when it returned */
    int  counts[4];
    int  vtop_in, vtop_out;
    Value top;
    uint64_t wide;              /* a VAL_WIDE top's bits */
} GenRec;

extern GenRec *gl_log;
extern int     gl_n;
extern int     gl_fn;
const char *gl_kept(long at);

/* ssa.c: the function the log describes, built as SSA and generated from
 * that, with the backend wound back to where the function began. Answers
 * 0, with nothing emitted and *why saying what it could not do, when the
 * function uses something the builder does not handle yet. */
int ssa_generate(const char **why);

/* And what the code it made would cost to run, against what the first
 * pass made: bytes, each block's counted eight times over for each loop
 * around it. Both 0 when it did not say. */
extern long ssa_cost_made, ssa_cost_first;
extern int ssa_size_made, ssa_size_first;
extern int ssa_made_leaf, ssa_leaf_off, ssa_cached_refused;

/* The machine-level backend (mir.c): the way being made is its, and the
 * code just made was. */
extern int ssa_mir_want, ssa_made_mir;
extern int ssa_cache_off, ssa_cached_used;
/* ssa_cache_off's values: which locals whose address is taken are cached */
enum { CACHE_ALL, CACHE_NONE, CACHE_LOOPS };
void ssa_restore(void);
void ssa_keep_moves(void);

/* The code the first pass made, kept by gl_function_end before it winds
 * the backend back: where it began, and its bytes. */
extern const unsigned char *gl_first_code;
extern int gl_first_from, gl_first_len;

#endif

#endif
