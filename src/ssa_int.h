/*
 * opt-acc's SSA form, as ssa.c builds it, for the backends that read it:
 * ssa.c's own and mir.c's (docs/machine-ir-backend.md).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_SSA_INT_H
#define ACC_SSA_INT_H

#include <stdint.h>

#include "acc.h"
#include "gen_int.h"
#include "genlog.h"

/* An operand as it was on the stack: an SSA value, or a constant made again
 * where it is used; and its attributes there, which relabelling calls
 * (vset_type and the rest) may have changed since the value was made. */
typedef struct {
    int      val;               /* SSA value; S_CONST or S_VOID otherwise */
    Value    attr;              /* kind, type, val, ext, quals, bits */
    uint64_t wide;              /* a VAL_WIDE constant's bits */
} Ent;

#define S_CONST (-1)
#define S_VOID  (-2)

/* What an instruction is, when it is not a call of the log's. */
enum {
    I_BR = GL_COUNT,            /* jump when the operand's truth is `sense` */
    I_JMP,                      /* jump */
    I_SET,                      /* a phi's slot, from the operand */
    I_FRAME,                    /* a frame call, laid down with the prologue */
    I_CONV,                     /* a local's new value: the operand converted
                                 * to the local's type */
    I_STEP                      /* ++ or -- of a local's value */
};

#define MAX_OPERANDS 8

typedef struct {
    int op;                     /* GL_* or I_* */
    const GenRec *rec;          /* the call, for its arguments */
    int nin;
    Ent in[MAX_OPERANDS];       /* operands, bottom of the stack first */
    int res;                    /* the value defined, or -1 */
    int sense;                  /* I_BR: jump when the truth is this */
    int target;                 /* I_BR, I_JMP: the block; I_SET: the value */
    int block;
    int step_op;                /* I_STEP: TK_PLUS or TK_MINUS */
    Type local_type;            /* I_CONV, I_STEP: the local's type */
    unsigned char kills;        /* the operands this is the last read of, a
                                 * bit each (find_clashes) */
    unsigned char delegated;    /* leaf backend: made by the first pass's
                                 * code, for a wide value (leaf_delegate) */
    unsigned char wide;         /* leaf backend: a long's, made here in its
                                 * slots (leaf_wide) */
    unsigned char mem_store;    /* I_STEP: of a cached local, whose new
                                 * value is written to its memory too */
} Ins;

typedef struct {
    Type type;                  /* as it is stored: the type it was made at */
    int  slot;                  /* its frame slot */
    int  def, last;             /* the instructions that make and last use it */
    int  used;                  /* read at all: by an instruction or a phi */
    int  reg;                   /* with OPTACC_REGS: the register it lives
                                 * in, or HOME_SLOT for its frame slot */
    int  fwd;                   /* left on the classic stack for its one
                                 * use, never kept anywhere */
    int  mem;                   /* a cached local's memory, read where it is
                                 * read: the local's offset, or 0 */
    int  of_cached;             /* a cached local's value: the local + 1 */
} SVal;

#define HOME_SLOT (-1)
#define HOME_GLOBAL (-3)                /* leaf backend: a global's address,
                                         * loaded where it is read, `slot` its
                                         * symbol and leaf_global_off what is
                                         * added to it -- see emit_leaf */

typedef struct {
    int first;                  /* its first instruction */
    int old_at;                 /* where it started in the first pass */
} Block;

#define GROW(arr, count, cap) do {                                      \
        if ((count) == (cap)) {                                         \
            (cap) = (cap) ? (cap) * 2 : 64;                             \
            (arr) = realloc((arr), (size_t) (cap) * sizeof *(arr));     \
            if (!(arr))                                                 \
                acc_error("out of memory for the SSA form");            \
        }                                                               \
    } while (0)

#define MAX_LOCALS 256           /* zap's biggest have more than 64 */
#define UNDEF (-3)              /* no value reaches: a read before any write */

typedef struct {
    int  offset;
    Type type;
    int  ext;
    int  ok;                    /* still a candidate */
    int  is_param;
    int  entry_val;             /* a parameter's value as the function begins */
    int  mem_val;               /* cached: its memory as a value, for a phi
                                 * on an edge where nothing is known */
    int  cached;                /* its address taken, so memory still, but read
                                 * from the value last read or written there
                                 * until something may have written it: see
                                 * cache_barrier */
} Local;

/* A phi: the local it joins, the value it makes, and what each of its
 * block's predecessors brings -- a value, or UNDEF. */
typedef struct {
    int block, local, val;
    int *in;                    /* by predecessor, in preds[block] order */
    int live;
} Phi;

/* The CFG, a list a block: successors and predecessors. */
typedef struct {
    int *at, count, cap;
} IntList;

/* The form, ssa.c's: see there. */
extern Ins     *insns;
extern int      ninsns;
extern SVal    *vals;
extern int      nvals;
extern Block   *blocks;
extern int      nblocks;
extern Phi     *phis;
extern int      nphis;
extern IntList *succs, *preds;
extern int     *rpo_num, *block_last, *loop_depth;
extern Local    locals[MAX_LOCALS];
extern int      nlocals;
extern int      cached_any;
extern int      blocks_from;
extern int      nraws, nmerged;
extern const char *fail;

void call(const Ins *insn);
int  frame_of_value(const Ins *insn);
void frame_again(const Ins *insn);
void frame_again_all(int arrays_here);
int  array_moved(int array);
void ssa_inline_slots(void);
void ssa_emit_statics(void);
int  ssa_inline_bytes(void);
int  ssa_locals_kept(void);
void costs(const int *block_start, int end);
int  inline_moved(int offset);
void find_loops(void);
int  block_dominates(int over, int blk);   /* after find_loops */

/* mir.c's: the machine-level backend (ssa_mir_want, ssa_made_mir: see
 * genlog.h). */
int  mir_on(void);
int  mir_build(void);
void mir_emit(void);
const char *mir_reason(void);
int  mir_parts(void);           /* the parts the last split made, or 0 */
int  ssa_moved_at(int at);      /* where what was at `at` is now */

#endif
