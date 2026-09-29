/*
 * opt-acc's SSA form of a function, built from the log of the parser's
 * calls into the code generator (genlog.c), and code generated from it.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The log is a stack machine: each call takes values from the stack the
 * parser has been pushing and may leave one. Walked with a stack of its
 * own, every value the log makes becomes an SSA value, every call that uses
 * values names them as operands, and the control flow -- jumps and labels,
 * && and ||, ?:, switch, return -- becomes basic blocks and the edges
 * between them. What each value is, its type and the rest, is what the
 * classic backend made of it the first time, which the log recorded: the
 * type rules are not written twice.
 *
 * This is milestone 2 of docs/optimizer-plan.md: the form, and a plain way
 * to generate code from it. Locals stay in memory; a value lives in a frame
 * slot of its own from where it is made to its last use, and a phi is a
 * slot its predecessors store into. Code comes out through gen.h again,
 * one call per SSA instruction, with its operands loaded from their slots
 * -- worse code than the classic backend's, since nothing stays in a
 * register, but code whose correctness says the form is right. The
 * optimisations and the register allocator come after this.
 *
 * A function that uses something the builder does not handle yet -- a
 * struct as a value, an array whose length is known only when it runs,
 * code inlined in place, a static local's bytes -- is left to the classic
 * backend, and ssa_generate says why.
 */
#ifdef OPT_ACC

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "gen_int.h"
#include "out_int.h"
#include "genlog.h"

#define GENLOG_OPS
#include "genlog_calls.h"
#undef GENLOG_OPS

/* ------------------------------------------------------------------ */
/* the form                                                            */

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
} SVal;

#define HOME_SLOT (-1)

typedef struct {
    int first;                  /* its first instruction */
    int old_at;                 /* where it started in the first pass */
} Block;

static Ins   *insns;
static int    ninsns, insns_cap;
static SVal  *vals;
static int    nvals, vals_cap;
static Block *blocks;
static int    nblocks, blocks_cap;

static const char *fail;        /* why the function is left to the classic */

static int frame_of_value(const Ins *insn);

/* Webs of values that share one home: see coalesce. */
static int  web_root(int val);
static int *web_next;

/* A hole the first pass returned, and the instruction whose edge it is; or,
 * with `hole` negative, a ?: whose answer is parked: -1 - its value, and in
 * `insn` the hole of the jump past the third operand. */
typedef struct {
    int hole, insn;
} Hole;

static Hole *holes;
static int   nholes, holes_cap;

/* A position gen_here returned, and the block that starts there. */
typedef struct {
    int at, block;
} Here;

static Here *heres;
static int   nheres, heres_cap;

#define GROW(arr, count, cap) do {                                      \
        if ((count) == (cap)) {                                         \
            (cap) = (cap) ? (cap) * 2 : 64;                             \
            (arr) = realloc((arr), (size_t) (cap) * sizeof *(arr));     \
            if (!(arr))                                                 \
                acc_error("out of memory for the SSA form");            \
        }                                                               \
    } while (0)

static Ent *stk;                /* the builder's value stack */
static int  nstk, stk_cap;

static int new_block(int old_at)
{
    GROW(blocks, nblocks, blocks_cap);
    blocks[nblocks].first = ninsns;
    blocks[nblocks].old_at = old_at;

    return nblocks++;
}

static Ins *new_insn(int op, const GenRec *rec)
{
    Ins *insn;

    GROW(insns, ninsns, insns_cap);
    insn = &insns[ninsns++];
    memset(insn, 0, sizeof *insn);
    insn->op = op;
    insn->rec = rec;
    insn->res = -1;
    insn->target = -1;
    insn->block = nblocks - 1;

    return insn;
}

static int new_val(Type type, int def)
{
    GROW(vals, nvals, vals_cap);
    vals[nvals].type = type;
    vals[nvals].slot = 0;
    vals[nvals].def = def;
    vals[nvals].last = def;
    vals[nvals].used = 0;
    vals[nvals].reg = HOME_SLOT;
    vals[nvals].fwd = 0;

    return nvals++;
}

static void push(Ent ent)
{
    GROW(stk, nstk, stk_cap);
    stk[nstk++] = ent;
}

static Ent pop(void)
{
    if (nstk == 0) {
        Ent none = { S_VOID, { 0 }, 0 };

        fail = "the stack went below empty";
        return none;
    }

    return stk[--nstk];
}

/* The operands of the instruction just made: the top `count` of the stack. */
static void take(Ins *insn, int count)
{
    int at;

    if (count > MAX_OPERANDS || count > nstk) {
        fail = count > MAX_OPERANDS ? "a call with more than eight operands"
                                    : "the stack went below empty";
        return;
    }
    insn->nin = count;
    for (at = 0; at != count; at++)
        insn->in[at] = stk[nstk - count + at];
    nstk -= count;
}

/* The entry a pure call left on top, as the first pass has it: a constant
 * to make again where it is used, nothing (void), or the value `val`. */
static Ent result(const GenRec *rec, int val)
{
    Ent ent;

    ent.attr = rec->top;
    ent.wide = rec->wide;
    if (val_const(rec->top.kind) || rec->top.kind == VAL_WIDE)
        ent.val = S_CONST;
    else if (rec->top.kind == VAL_VOID)
        ent.val = S_VOID;
    else
        ent.val = val;

    return ent;
}

static Ent constant(int number)
{
    Ent ent;

    memset(&ent, 0, sizeof ent);
    ent.val = S_CONST;
    ent.attr.kind = VAL_CONST;
    ent.attr.type = TY_INT;
    ent.attr.val = number;

    return ent;
}

static void hole_of(int hole, int insn_at)
{
    if (!hole)
        return;
    GROW(holes, nholes, holes_cap);
    holes[nholes].hole = hole;
    holes[nholes].insn = insn_at;
    nholes++;
}

/* The block a new label begins: the current one, if nothing is in it yet;
 * else a new one. */
static int label_block(int old_at)
{
    Block *last = &blocks[nblocks - 1];

    if (last->first == ninsns) {
        last->old_at = old_at;
        return nblocks - 1;
    }

    return new_block(old_at);
}

/* A hole's jump lands in `block`. */
static void land(int hole, int block)
{
    int at;

    if (!hole)
        return;
    for (at = 0; at != nholes; at++)
        if (holes[at].hole == hole && holes[at].insn >= 0) {
            insns[holes[at].insn].target = block;
            holes[at].insn = -1;
            return;
        }
    fail = "a label for a jump the log did not make";
}

static int block_at(int old_at)
{
    int at;

    for (at = nheres - 1; at >= 0; at--)
        if (heres[at].at == old_at)
            return heres[at].block;
    fail = "a jump back to a place gen_here did not give";

    return 0;
}

/* Whether a call is a query or a check, which changes nothing. */
static int query(int op)
{
    switch (op) {
    case GL_vtype: case GL_vext: case GL_vconst_top: case GL_vconst_wide:
    case GL_vbits: case GL_vquals: case GL_vtype_at: case GL_vconst_addr:
    case GL_vconst_bss: case GL_gen_nwants: case GL_gen_want_name:
    case GL_gen_local_fits: case GL_gen_iy_pick: case GL_vpop_reg:
    case GL_gen_cond_same:
        return 1;
    }

    return 0;
}

/* Whether a call only relabels the value on top. */
static int relabel(int op)
{
    switch (op) {
    case GL_vset_addr: case GL_vset_type: case GL_vset_ext:
    case GL_vset_quals: case GL_vset_bits:
        return 1;
    }

    return 0;
}

/* Whether a call is the frame's: laid down again before the body. */
static int frame(int op)
{
    switch (op) {
    case GL_gen_local: case GL_gen_local_far: case GL_gen_local_array:
    case GL_gen_local_array_size: case GL_gen_iy_claim:
    case GL_gen_iy_param: case GL_gen_iy_take:
        return 1;
    }

    return 0;
}

/* Whether a call makes a value and does nothing else, so that when the
 * value comes out a constant the constant is the whole of it. */
static int pure(int op)
{
    switch (op) {
    case GL_vapply: case GL_vconvert: case GL_vcast: case GL_vneg:
    case GL_vnot: case GL_vtruth: case GL_vmember: case GL_vpush_const:
    case GL_vpush_bss: case GL_vpush_const_long: case GL_vpush_const_wide:
    case GL_vpush_const_float:
        return 1;
    }

    return 0;
}

/* Whether a call leaves a value on the stack. */
static int pushes(int op)
{
    switch (op) {
    case GL_vdrop: case GL_gen_discard: case GL_gen_jump_if_false:
    case GL_gen_jump_if_true_to: case GL_gen_logic_left:
    case GL_gen_cond_begin: case GL_gen_cond_middle:
    case GL_gen_cond_middle_void: case GL_gen_return:
    case GL_gen_stmt_end: case GL_gen_value_end: case GL_gen_jump:
    case GL_gen_jump_to: case GL_gen_label: case GL_gen_here:
    case GL_gen_switch_load: case GL_gen_switch_case:
    case GL_gen_zero_array: case GL_gen_copy_to_array: case GL_gen_data:
    case GL_gen_cond_same: case GL_gen_func_begin:
        return 0;
    }

    return 1;
}

/* && and ||, after the right operand: the answer is `settles` if either
 * side settled it and !settles if neither did. */
static void build_logic_right(const GenRec *rec)
{
    int settles = (int) rec->arg[0], answer, settled, jump_at, branch_at;
    Ins *branch, *set;

    /* Kept by index: a new instruction may move the array. */
    branch = new_insn(I_BR, rec);
    branch_at = ninsns - 1;
    take(branch, 1);
    branch->sense = settles;
    answer = new_val(rec->top.type, ninsns - 1);

    new_block(-1);                          /* neither side settled it */
    set = new_insn(I_SET, rec);
    set->nin = 1;
    set->in[0] = constant(!settles);
    set->target = answer;
    new_insn(I_JMP, rec);
    jump_at = ninsns - 1;

    settled = new_block(-1);                /* one of them did */
    insns[branch_at].target = settled;
    land((int) rec->arg[1], settled);
    set = new_insn(I_SET, rec);
    set->nin = 1;
    set->in[0] = constant(settles);
    set->target = answer;

    insns[jump_at].target = new_block(-1);
    push(result(rec, answer));
}

/* ?:, after the middle operand: its value into the answer's slot, which is
 * made now and given its type when the third operand says what it is. */
static void build_cond_middle(const GenRec *rec)
{
    int is_void = rec->op == GL_gen_cond_middle_void;
    int answer = new_val(TY_VOID, ninsns);
    Ins *set = new_insn(I_SET, rec);

    take(set, 1);
    set->target = is_void ? -1 : answer;
    if (is_void)
        set->op = GL_vdrop;
    new_insn(I_JMP, rec);
    hole_of((int) rec->ret, ninsns - 1);
    GROW(holes, nholes, holes_cap);
    holes[nholes].hole = -1 - answer;
    holes[nholes].insn = (int) rec->ret;
    nholes++;
    new_block(-1);
}

/* ?:, after the third operand: its value into the answer's slot too, and
 * the join where the answer is read. */
static void build_cond_end(const GenRec *rec)
{
    int to_stub = (int) rec->arg[0], answer = -1, at, join;
    Ins *set;

    for (at = nholes - 1; at >= 0; at--)
        if (holes[at].hole < 0 && holes[at].insn == to_stub) {
            answer = -1 - holes[at].hole;
            holes[at].insn = 0;
            break;
        }
    set = new_insn(I_SET, rec);
    take(set, 1);
    if (rec->op == GL_gen_cond_end_void || answer < 0) {
        set->op = GL_vdrop;
    } else {
        set->target = answer;
        vals[answer].type = rec->top.type;
    }
    join = new_block(-1);
    land(to_stub, join);
    push(result(rec, rec->op == GL_gen_cond_end_void ? -1 : answer));
}

/* A jump back to where gen_here was: the block that begins there. */
static int back_to(long old_at)
{
    return block_at((int) old_at);
}

static void build_one(const GenRec *rec)
{
    int op = rec->op, pops, block;
    Ins *insn;
    Ent ent;

    if (query(op))
        return;
    if (relabel(op)) {
        if (!nstk) {
            fail = "a relabel of nothing";
            return;
        }
        stk[nstk - 1].attr = rec->top;
        stk[nstk - 1].wide = rec->wide;
        return;
    }
    if (frame(op)) {
        new_insn(I_FRAME, rec);
        return;
    }
    pops = rec->vtop_in - rec->vtop_out + pushes(op);

    switch (op) {
    case GL_RAW:
        fail = "a static local's bytes";
        return;
    case GL_gen_inline_begin: case GL_gen_inline_end:
        fail = "code inlined in place";
        return;
    case GL_gen_stack_take: case GL_gen_stack_mark: case GL_gen_stack_back:
        fail = "an array whose length is known when it runs";
        return;
    case GL_gen_data_begin: case GL_gen_data_end: case GL_gen_pending_clear:
    case GL_gen_data_fixup: case GL_gen_bss_symbol: case GL_gen_bss_reserve:
    case GL_gen_bss_reserve_aligned: case GL_gen_bss_move:
    case GL_gen_bss_forget: case GL_gen_bss_fixup: case GL_gen_settle:
    case GL_gen_late_fixup: case GL_gen_slot: case GL_gen_link_fixup:
    case GL_out_rewind: case GL_out_seek:
        fail = "a static local";
        return;
    case GL_gen_mark: case GL_gen_rollback:
        fail = "internal: a mark left in the log";
        return;
    case GL_vpush_reg:
        fail = "a register pushed by the parser";
        return;

    case GL_vdup:
        if (!nstk) {
            fail = "vdup of nothing";
            return;
        }
        push(stk[nstk - 1]);
        return;
    case GL_vswap:
        if (nstk < 2) {
            fail = "vswap of fewer than two";
            return;
        }
        ent = stk[nstk - 1];
        stk[nstk - 1] = stk[nstk - 2];
        stk[nstk - 2] = ent;
        return;
    case GL_vdrop: case GL_gen_discard:
        while (pops-- > 0)
            (void) pop();
        return;

    case GL_gen_here:
        block = label_block((int) rec->ret);
        GROW(heres, nheres, heres_cap);
        heres[nheres].at = (int) rec->ret;
        heres[nheres].block = block;
        nheres++;
        return;
    case GL_gen_label:
        if (rec->arg[0])
            land((int) rec->arg[0], label_block(rec->at_after));
        return;
    case GL_gen_jump:
        new_insn(I_JMP, rec);
        hole_of((int) rec->ret, ninsns - 1);
        new_block(-1);
        return;
    case GL_gen_jump_to:
        insn = new_insn(I_JMP, rec);
        insn->target = back_to(rec->arg[0]);
        new_block(-1);
        return;
    case GL_gen_jump_if_false:
    case GL_gen_logic_left:
    case GL_gen_cond_begin:
        /* Popped, and a jump when its truth is the one that goes: false
         * for an if, a loop or ?:, `settles` for && and ||. None when the
         * first pass found the condition constant and did not need one. */
        insn = new_insn(I_BR, rec);
        take(insn, 1);
        insn->sense = op == GL_gen_logic_left ? (int) rec->arg[0] : 0;
        if (rec->ret)
            hole_of((int) rec->ret, ninsns - 1);
        else
            insn->op = GL_vdrop;
        new_block(-1);
        return;
    case GL_gen_jump_if_true_to:
        insn = new_insn(I_BR, rec);
        take(insn, 1);
        insn->sense = 1;
        insn->target = back_to(rec->arg[0]);
        new_block(-1);
        return;
    case GL_gen_logic_right:
        build_logic_right(rec);
        return;
    case GL_gen_cond_middle: case GL_gen_cond_middle_void:
        build_cond_middle(rec);
        return;
    case GL_gen_cond_end: case GL_gen_cond_end_void:
        build_cond_end(rec);
        return;
    case GL_gen_switch_case:
        insn = new_insn(op, rec);
        insn->target = back_to(rec->arg[3]);
        return;
    case GL_gen_return:
        insn = new_insn(op, rec);
        take(insn, pops);
        new_block(-1);
        return;
    }

    /* Everything else is a call made again with its operands. What a pure
     * call makes is a value or a constant; what any other makes is a value,
     * even when it comes out a constant -- a function's address, say, has a
     * fixup the constant alone would not carry. */
    insn = new_insn(op, rec);
    take(insn, pops);
    if (op == GL_vcast && insn->nin == 1 && insn->in[0].val == S_VOID) {
        insn->op = GL_vdrop;            /* (void) of what is already void */
        push(insn->in[0]);
        return;
    }
    if (!pushes(op))
        return;
    if (rec->vtop_out == 0) {
        fail = "a call that should leave a value and left none";
        return;
    }
    if ((val_const(rec->top.kind) || rec->top.kind == VAL_WIDE) && pure(op)) {
        insn->op = GL_vdrop;            /* folded: nothing to emit */
        push(result(rec, -1));
        return;
    }
    if (type_is_struct(rec->top.type)) {
        fail = "a struct as a value";
        return;
    }
    if (rec->top.kind == VAL_IYADDR) {
        fail = "the address of the local in IY";
        return;
    }
    if (rec->top.kind == VAL_VOID) {
        push(result(rec, -1));
        return;
    }
    insn->res = new_val(rec->top.type, ninsns - 1);
    ent.val = insn->res;
    ent.attr = rec->top;
    ent.wide = rec->wide;
    push(ent);
}

/* Which records to build from: all but what sizeof compiled and took back
 * -- a mark and everything up to its rollback -- except the frame's calls,
 * whose room stays taken. */
static void keep_records(const GenRec *log, int count, int *keep)
{
    int rec_at, back_at;

    for (rec_at = 0; rec_at != count; rec_at++)
        keep[rec_at] = 1;
    for (rec_at = count - 1; rec_at >= 0; rec_at--) {
        if (log[rec_at].op != GL_gen_rollback)
            continue;
        for (back_at = (int) log[rec_at].arg[0]; back_at <= rec_at; back_at++)
            if (!frame(log[back_at].op))
                keep[back_at] = 0;
    }
    for (rec_at = 0; rec_at != count; rec_at++)
        if (log[rec_at].op == GL_gen_mark)
            keep[rec_at] = 0;
}

/* The answer of && or || -- a 1 or a 0 set on each path in, and read by a
 * branch where the paths join -- is what a condition mostly is, and the
 * classic backend jumps where the answer would send it instead of making
 * it. So does this: each path that sets a constant answer jumps on to where
 * the branch goes with that answer, and a branch that nothing reaches
 * with an answer any more goes. Made again until nothing changes, for
 * `a && b && c`, whose answers feed each other. */
static int ignorable(const Ins *insn)
{
    return (insn->op == GL_vdrop && insn->nin == 0)
           || insn->op == GL_gen_stmt_end;
}

static void thread_answers(void)
{
    int *uses = malloc(((size_t) nvals + 1) * sizeof *uses);
    int changed = 1, at, operand;

    if (!uses)
        acc_error("out of memory for the SSA form");
    while (changed) {
        changed = 0;
        memset(uses, 0, ((size_t) nvals + 1) * sizeof *uses);
        for (at = 0; at != ninsns; at++)
            for (operand = 0; operand != insns[at].nin; operand++)
                if (insns[at].in[operand].val >= 0)
                    uses[insns[at].in[operand].val]++;

        for (at = 0; at != ninsns; at++) {
            Ins *branch = &insns[at];
            int join = branch->block, answer, first, set_at, left = 0;

            if (branch->op != I_BR || branch->nin != 1 || branch->target < 0)
                continue;
            answer = branch->in[0].val;
            if (answer < 0 || uses[answer] != 1 || join + 1 >= nblocks)
                continue;
            for (first = blocks[join].first; first != at; first++)
                if (!ignorable(&insns[first]))
                    break;
            if (first != at)
                continue;               /* the branch is not all the block does */

            for (set_at = 0; set_at != ninsns; set_at++) {
                Ins *set = &insns[set_at];
                int where, truth;

                if (set->op != I_SET || set->target != answer)
                    continue;
                if (set->in[0].val != S_CONST || set->in[0].attr.kind != VAL_CONST) {
                    left++;
                    continue;
                }
                truth = set->in[0].attr.val != 0;
                where = truth == branch->sense ? branch->target : join + 1;
                if (set_at + 1 < ninsns && insns[set_at + 1].block == set->block
                    && insns[set_at + 1].op == I_JMP
                    && insns[set_at + 1].target == join) {
                    insns[set_at + 1].target = where;
                    set->op = GL_vdrop;
                    set->nin = 0;
                    set->target = -1;
                } else if (set->block + 1 == join
                           && (set_at + 1 == ninsns
                               || insns[set_at + 1].block != set->block)) {
                    set->op = I_JMP;    /* it fell into the join */
                    set->nin = 0;
                    set->target = where;
                } else {
                    left++;
                    continue;
                }
                changed = 1;
            }
            if (!left) {
                branch->op = GL_vdrop;  /* nothing reaches it with an answer */
                branch->nin = 0;
                branch->target = -1;
                changed = 1;
            }
        }
    }
    free(uses);
}

/* ------------------------------------------------------------------ */
/* locals as values                                                    */

/* A local or parameter the function reads and writes only by name -- its
 * address never taken, always the same scalar type -- stops being memory:
 * each store to it makes a new SSA value, each read uses the one that
 * reaches it, and where paths with different values meet, a phi joins
 * them. What is left of the local is its values; its slot is not used. */

#define MAX_LOCALS 64
#define UNDEF (-3)              /* no value reaches: a read before any write */

typedef struct {
    int  offset;
    Type type;
    int  ext;
    int  ok;                    /* still a candidate */
    int  is_param;
    int  entry_val;             /* a parameter's value as the function begins */
} Local;

static Local locals[MAX_LOCALS];
static int   nlocals;

/* A phi: the local it joins, the value it makes, and what each of its
 * block's predecessors brings -- a value, or UNDEF. */
typedef struct {
    int block, local, val;
    int *in;                    /* by predecessor, in preds[block] order */
    int live;
} Phi;

static Phi *phis;
static int  nphis, phis_cap;

/* The CFG, a list a block: successors and predecessors. */
typedef struct {
    int *at, count, cap;
} IntList;

static IntList *succs, *preds, *dom_kids;
static int     *idom, *rpo_num, *block_last;

static int *repl;               /* a value read in place of another, or -1 */

static void list_add(IntList *list, int item)
{
    GROW(list->at, list->count, list->cap);
    list->at[list->count++] = item;
}

static int local_of(int offset)
{
    int at;

    for (at = 0; at != nlocals; at++)
        if (locals[at].offset == offset)
            return at;

    return -1;
}

/* A local seen with `type`: added, or ruled out if seen with another. */
static void local_seen(int offset, Type type, int ext)
{
    int at = local_of(offset);

    if (at < 0) {
        if (nlocals == MAX_LOCALS)
            return;
        at = nlocals++;
        locals[at].offset = offset;
        locals[at].type = type;
        locals[at].ext = ext;
        locals[at].ok = !type_is_struct(type);
        locals[at].is_param = offset > 0;
        locals[at].entry_val = -1;
        return;
    }
    if (locals[at].type != type)
        locals[at].ok = 0;
}

static void local_ruled_out(int offset)
{
    int at = local_of(offset);

    if (at < 0 && nlocals < MAX_LOCALS) {
        at = nlocals++;
        locals[at].offset = offset;
        locals[at].type = TY_VOID;
    }
    if (at >= 0)
        locals[at].ok = 0;
}

/* Whether a value's type is an array whose length is known when it runs,
 * or a pointer to one: the backend reads the length from a frame slot the
 * type names, where the log does not see it. */
static int runtime_array(Type type, int ext)
{
    while (type_pointer(type))
        type = type_deref(type);

    return type_is_array(type) && ext_vla_size(ext) != 0;
}

/* Which locals can become values. None of them, in a function with an
 * array whose length is known when it runs: its length is in a local that
 * the backend reads without a call the log would show. */
static void find_locals(void)
{
    int at;

    nlocals = 0;
    for (at = 0; at != ninsns; at++)
        if (insns[at].rec && runtime_array(insns[at].rec->top.type,
                                           insns[at].rec->top.ext))
            return;
    for (at = 0; at != ninsns; at++) {
        const Ins *insn = &insns[at];
        const GenRec *rec = insn->rec;

        switch (insn->op) {
        case GL_vpush_local: case GL_vstore_local:
            local_seen((int) rec->arg[0], (Type) rec->arg[1],
                       rec->top.ext);
            break;
        case GL_vprefix_local: case GL_vpostfix_local:
            local_seen((int) rec->arg[0], (Type) rec->arg[1],
                       (int) rec->arg[2]);
            break;
        case GL_vaddr_local:
            local_ruled_out((int) rec->arg[0]);
            break;
        case GL_gen_switch_load: case GL_gen_switch_case:
            local_ruled_out(insn->op == GL_gen_switch_load
                            ? (int) rec->arg[0] : (int) rec->arg[4]);
            break;
        case GL_gen_call:
            /* longjmp comes back to setjmp with the frame as it is, and a
             * value that moved out of its local would not be there. */
            if (!strcmp(name_text(sym_at((int) rec->arg[0])->name), "setjmp"))
                nlocals = MAX_LOCALS + 1;
            break;
        default:
            break;
        }
        if (nlocals > MAX_LOCALS)
            break;
    }
    if (nlocals > MAX_LOCALS)
        nlocals = 0;
}

/* The blocks each block goes on to, and where each block's code ends. */
static void build_cfg(void)
{
    int blk, at;

    succs = calloc((size_t) nblocks, sizeof *succs);
    preds = calloc((size_t) nblocks, sizeof *preds);
    dom_kids = calloc((size_t) nblocks, sizeof *dom_kids);
    block_last = malloc((size_t) nblocks * sizeof *block_last);
    if (!succs || !preds || !dom_kids || !block_last)
        acc_error("out of memory for the SSA form");
    for (blk = 0; blk != nblocks; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;
        int falls = 1;

        block_last[blk] = end - 1;
        for (at = blocks[blk].first; at != end; at++) {
            const Ins *insn = &insns[at];

            if ((insn->op == I_BR || insn->op == I_JMP
                 || insn->op == GL_gen_switch_case) && insn->target >= 0)
                list_add(&succs[blk], insn->target);
            if (at == end - 1
                && (insn->op == I_JMP || insn->op == GL_gen_return))
                falls = 0;
        }
        if (falls && blk + 1 < nblocks)
            list_add(&succs[blk], blk + 1);
    }
    for (blk = 0; blk != nblocks; blk++)
        for (at = 0; at != succs[blk].count; at++)
            list_add(&preds[succs[blk].at[at]], blk);
}

/* Reverse postorder from the entry, and each block's immediate dominator
 * (Cooper, Harvey and Kennedy's iteration). A block the entry does not
 * reach has none. */
static int *rpo;
static int  nrpo;

static void postorder(int blk, int *seen)
{
    int at;

    seen[blk] = 1;
    for (at = 0; at != succs[blk].count; at++)
        if (!seen[succs[blk].at[at]])
            postorder(succs[blk].at[at], seen);
    rpo[nrpo++] = blk;
}

static int intersect(int one, int two)
{
    while (one != two) {
        while (rpo_num[one] > rpo_num[two])
            one = idom[one];
        while (rpo_num[two] > rpo_num[one])
            two = idom[two];
    }

    return one;
}

static void dominators(void)
{
    int *seen = calloc((size_t) nblocks, sizeof *seen);
    int at, changed;

    rpo = malloc((size_t) nblocks * sizeof *rpo);
    rpo_num = malloc((size_t) nblocks * sizeof *rpo_num);
    idom = malloc((size_t) nblocks * sizeof *idom);
    if (!seen || !rpo || !rpo_num || !idom)
        acc_error("out of memory for the SSA form");
    nrpo = 0;
    postorder(0, seen);
    for (at = 0; at != nrpo / 2; at++) {
        int swap = rpo[at];

        rpo[at] = rpo[nrpo - 1 - at];
        rpo[nrpo - 1 - at] = swap;
    }
    for (at = 0; at != nblocks; at++) {
        rpo_num[at] = -1;
        idom[at] = -1;
    }
    for (at = 0; at != nrpo; at++)
        rpo_num[rpo[at]] = at;
    idom[0] = 0;
    do {
        changed = 0;
        for (at = 1; at != nrpo; at++) {
            int blk = rpo[at], pred, dom = -1;

            for (pred = 0; pred != preds[blk].count; pred++) {
                int from = preds[blk].at[pred];

                if (idom[from] < 0)
                    continue;
                dom = dom < 0 ? from : intersect(from, dom);
            }
            if (dom >= 0 && idom[blk] != dom) {
                idom[blk] = dom;
                changed = 1;
            }
        }
    } while (changed);
    for (at = 1; at != nrpo; at++)
        list_add(&dom_kids[idom[rpo[at]]], rpo[at]);
    free(seen);
}

/* The blocks where each local needs a phi: the iterated dominance frontier
 * of the blocks that write it. */
static void place_phis(void)
{
    IntList *frontier = calloc((size_t) nblocks, sizeof *frontier);
    int *writes = calloc((size_t) nblocks, sizeof *writes);
    int *has_phi = calloc((size_t) nblocks, sizeof *has_phi);
    int *work = malloc(((size_t) nblocks + 1) * sizeof *work);
    int blk, at, local, pred;

    if (!frontier || !writes || !has_phi || !work)
        acc_error("out of memory for the SSA form");
    for (blk = 0; blk != nblocks; blk++) {
        if (rpo_num[blk] < 0 || preds[blk].count < 2)
            continue;
        for (pred = 0; pred != preds[blk].count; pred++) {
            int runner = preds[blk].at[pred];

            if (rpo_num[runner] < 0)
                continue;
            while (runner != idom[blk]) {
                list_add(&frontier[runner], blk);
                runner = idom[runner];
            }
        }
    }
    for (local = 0; local != nlocals; local++) {
        int nwork = 0;

        if (!locals[local].ok)
            continue;
        memset(writes, 0, (size_t) nblocks * sizeof *writes);
        memset(has_phi, 0, (size_t) nblocks * sizeof *has_phi);
        if (locals[local].is_param) {
            writes[0] = 1;
            work[nwork++] = 0;
        }
        for (at = 0; at != ninsns; at++) {
            const Ins *insn = &insns[at];

            if ((insn->op == GL_vstore_local || insn->op == GL_vprefix_local
                 || insn->op == GL_vpostfix_local)
                && insn->rec->arg[0] == locals[local].offset
                && !writes[insn->block] && rpo_num[insn->block] >= 0) {
                writes[insn->block] = 1;
                work[nwork++] = insn->block;
            }
        }
        while (nwork) {
            blk = work[--nwork];
            for (at = 0; at != frontier[blk].count; at++) {
                int join = frontier[blk].at[at];

                if (has_phi[join])
                    continue;
                has_phi[join] = 1;
                GROW(phis, nphis, phis_cap);
                phis[nphis].block = join;
                phis[nphis].local = local;
                phis[nphis].val = new_val(locals[local].type,
                                          blocks[join].first);
                phis[nphis].in = malloc(((size_t) preds[join].count + 1)
                                        * sizeof (int));
                if (!phis[nphis].in)
                    acc_error("out of memory for the SSA form");
                for (pred = 0; pred != preds[join].count; pred++)
                    phis[nphis].in[pred] = UNDEF;
                phis[nphis].live = 0;
                nphis++;
                if (!writes[join]) {
                    writes[join] = 1;
                    work[nwork++] = join;
                }
            }
        }
    }
    for (blk = 0; blk != nblocks; blk++)
        free(frontier[blk].at);
    free(frontier);
    free(writes);
    free(has_phi);
    free(work);
}

static int find_val(int val)
{
    while (val >= 0 && repl[val] >= 0 && repl[val] != val)
        val = repl[val];

    return val;
}

/* The operand an instruction reads, with what it stood for resolved. */
static void resolve(Ent *ent)
{
    if (ent->val < 0)
        return;
    ent->val = find_val(ent->val);
    if (ent->val == UNDEF) {
        ent->val = S_CONST;             /* a read before any write: 0 */
        ent->attr.kind = VAL_CONST;
        ent->attr.val = 0;
        ent->wide = 0;
    }
}

/* Each block, in the dominator tree's order: every read of a local turned
 * into the value that reaches it, every write into a new value, and each
 * successor's phis told what this block brings them. */
static int *cur_def;            /* by local: a stack of values, one a push */
static int *cur_top;
static int  cur_cap;

static void rename_block(int blk)
{
    int pushed[MAX_LOCALS], at, end, succ, phi, kid;

    memset(pushed, 0, sizeof pushed);
#define DEF_PUSH(local, val) do {                                        \
        cur_def[(local) * cur_cap + cur_top[local]++] = (val);           \
        pushed[local]++;                                                 \
    } while (0)
#define DEF_TOP(local) (cur_top[local] ? cur_def[(local) * cur_cap          \
                                                 + cur_top[local] - 1]    \
                                       : UNDEF)

    for (phi = 0; phi != nphis; phi++)
        if (phis[phi].block == blk)
            DEF_PUSH(phis[phi].local, phis[phi].val);
    end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;
    for (at = blocks[blk].first; at != end; at++) {
        Ins *insn = &insns[at];
        int local, operand;

        for (operand = 0; operand != insn->nin; operand++)
            resolve(&insn->in[operand]);
        if (insn->op != GL_vpush_local && insn->op != GL_vstore_local
            && insn->op != GL_vprefix_local && insn->op != GL_vpostfix_local)
            continue;
        local = local_of((int) insn->rec->arg[0]);
        if (local < 0 || !locals[local].ok)
            continue;

        if (insn->op == GL_vpush_local) {
            repl[insn->res] = DEF_TOP(local);
            insn->op = GL_vdrop;
            insn->nin = 0;
            insn->res = -1;
        } else if (insn->op == GL_vstore_local) {
            const Ent *stored = &insn->in[0];

            if (stored->val >= 0 && vals[stored->val].type == locals[local].type
                && stored->attr.type == locals[local].type) {
                repl[insn->res] = stored->val;
                DEF_PUSH(local, stored->val);
                insn->op = GL_vdrop;
                insn->nin = 0;
                insn->res = -1;
            } else {
                insn->op = I_CONV;
                insn->local_type = locals[local].type;
                vals[insn->res].type = locals[local].type;
                DEF_PUSH(local, insn->res);
            }
        } else {
            int old = DEF_TOP(local), result = insn->res;
            int stepped = new_val(locals[local].type, at);
            Ent operand_ent;

            memset(&operand_ent, 0, sizeof operand_ent);
            operand_ent.attr = insn->rec->top;
            operand_ent.attr.type = locals[local].type;
            operand_ent.attr.ext = (unsigned char) insn->rec->arg[2];
            operand_ent.attr.kind = VAL_LOCAL;
            operand_ent.val = old;
            resolve(&operand_ent);
            repl = realloc(repl, (size_t) nvals * sizeof *repl);
            if (!repl)
                acc_error("out of memory for the SSA form");
            repl[stepped] = -1;
            insn = &insns[at];
            insn->step_op = (int) insn->rec->arg[3];
            insn->local_type = locals[local].type;
            insn->nin = 1;
            insn->in[0] = operand_ent;
            insn->res = stepped;
            repl[result] = insn->op == GL_vprefix_local ? stepped : old;
            insn->op = I_STEP;
            DEF_PUSH(local, stepped);
        }
    }
    for (succ = 0; succ != succs[blk].count; succ++) {
        int to = succs[blk].at[succ], pred;

        for (pred = 0; pred != preds[to].count; pred++) {
            if (preds[to].at[pred] != blk)
                continue;
            for (phi = 0; phi != nphis; phi++)
                if (phis[phi].block == to)
                    phis[phi].in[pred] = DEF_TOP(phis[phi].local);
        }
    }
    for (kid = 0; kid != dom_kids[blk].count; kid++)
        rename_block(dom_kids[blk].at[kid]);
    for (at = 0; at != nlocals; at++)
        cur_top[at] -= pushed[at];
#undef DEF_PUSH
#undef DEF_TOP
}

/* Only the phis something reads are kept: a phi is live if an instruction
 * reads it, or a live phi does. */
static void live_phis(void)
{
    int at, operand, phi, changed;
    char *used = calloc((size_t) nvals + 1, 1);

    if (!used)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != ninsns; at++)
        for (operand = 0; operand != insns[at].nin; operand++)
            if (insns[at].in[operand].val >= 0)
                used[insns[at].in[operand].val] = 1;
    do {
        changed = 0;
        for (phi = 0; phi != nphis; phi++) {
            int pred;

            if (phis[phi].live || !used[phis[phi].val])
                continue;
            phis[phi].live = 1;
            changed = 1;
            for (pred = 0; pred != preds[phis[phi].block].count; pred++) {
                int from = find_val(phis[phi].in[pred]);

                phis[phi].in[pred] = from;
                if (from >= 0)
                    used[from] = 1;
            }
        }
    } while (changed);
    free(used);
}

static void to_values(void)
{
    int local, at;
    Phi *phi;

    nphis = 0;
    find_locals();
    build_cfg();
    dominators();
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param) {
            locals[local].entry_val = new_val(locals[local].type, 0);
        }
    place_phis();

    repl = malloc(((size_t) nvals + 1) * sizeof *repl);
    cur_cap = ninsns + nlocals + 8;
    cur_def = malloc(((size_t) nlocals + 1) * (size_t) cur_cap * sizeof *cur_def);
    cur_top = calloc((size_t) nlocals + 1, sizeof *cur_top);
    if (!repl || !cur_def || !cur_top)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != nvals; at++)
        repl[at] = -1;
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param)
            cur_def[local * cur_cap + cur_top[local]++] = locals[local].entry_val;
    rename_block(0);

    /* What the blocks the entry does not reach read, resolved as well. */
    for (at = 0; at != ninsns; at++) {
        int operand;

        for (operand = 0; operand != insns[at].nin; operand++)
            resolve(&insns[at].in[operand]);
    }
    live_phis();

    /* What the copies into phis cannot do yet: a phi wider than an int,
     * which a register cannot hold across them, and one a switch's case
     * jumps to, which leaves no place for its copies. */
    for (phi = phis; phi != phis + nphis && !fail; phi++) {
        int pred;

        if (!phi->live)
            continue;
        if (type_wide(vals[phi->val].type))
            fail = "a long or a float where paths join";
        for (pred = 0; pred != preds[phi->block].count && !fail; pred++) {
            int from = preds[phi->block].at[pred];

            for (at = blocks[from].first; at <= block_last[from]; at++)
                if (insns[at].op == GL_gen_switch_case
                    && insns[at].target == phi->block)
                    fail = "a switch's case where paths join";
        }
    }
    free(cur_def);
    free(cur_top);
    cur_def = cur_top = NULL;
}

/* Where each value is live, as the interval its slot is kept for: every
 * position from the first to the last at which it is live, found by the
 * usual dataflow over the blocks -- a value is live into a block that
 * reads it before writing it, and out of one that a successor needs it
 * live into, or whose successor's phi reads it. Layout order is not flow
 * order: a goto into a loop's middle puts a value's reads above where it
 * is made. */
static unsigned char *live_in, *live_out, *block_gen, *block_kill, *phi_def;

#define LIVE_BIT(set, blk, val) ((set)[(size_t) (blk) * (size_t) nvals + (val)])

static void widen(int val, int at)
{
    if (vals[val].def > at)
        vals[val].def = at;
    if (vals[val].last < at)
        vals[val].last = at;
}

static void live_ranges(void)
{
    size_t size = (size_t) nblocks * (size_t) nvals + 1;
    int blk, at, operand, val, phi_at, changed, local;

    live_in = calloc(size, 1);
    live_out = calloc(size, 1);
    block_gen = calloc(size, 1);
    block_kill = calloc(size, 1);
    phi_def = calloc(size, 1);
    if (!live_in || !live_out || !block_gen || !block_kill || !phi_def)
        acc_error("out of memory for the SSA form");

    /* What each block reads before it writes, and what it writes. */
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param)
            LIVE_BIT(block_kill, 0, locals[local].entry_val) = 1;
    for (phi_at = 0; phi_at != nphis; phi_at++)
        if (phis[phi_at].live) {
            LIVE_BIT(block_kill, phis[phi_at].block, phis[phi_at].val) = 1;
            LIVE_BIT(phi_def, phis[phi_at].block, phis[phi_at].val) = 1;
        }
    for (blk = 0; blk != nblocks; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;

        for (at = blocks[blk].first; at != end; at++) {
            const Ins *insn = &insns[at];

            for (operand = 0; operand != insn->nin; operand++) {
                val = insn->in[operand].val;
                if (val < 0)
                    continue;
                vals[val].used = 1;
                if (!LIVE_BIT(block_kill, blk, val))
                    LIVE_BIT(block_gen, blk, val) = 1;
            }
            if (insn->res >= 0)
                LIVE_BIT(block_kill, blk, insn->res) = 1;
            if (insn->op == I_SET && insn->target >= 0)
                LIVE_BIT(block_kill, blk, insn->target) = 1;
        }
    }
    for (phi_at = 0; phi_at != nphis; phi_at++) {
        const Phi *phi = &phis[phi_at];
        int pred;

        if (!phi->live)
            continue;
        for (pred = 0; pred != preds[phi->block].count; pred++)
            if (phi->in[pred] >= 0)
                vals[phi->in[pred]].used = 1;
    }

    /* Out of a block: what its successors need, and what their phis read
     * from it; into it: what it reads first, and what goes out that it
     * does not make. */
    do {
        changed = 0;
        for (blk = nblocks - 1; blk >= 0; blk--) {
            int succ;

            for (succ = 0; succ != succs[blk].count; succ++) {
                int to = succs[blk].at[succ], pred;

                for (val = 0; val != nvals; val++)
                    if (LIVE_BIT(live_in, to, val) && !LIVE_BIT(live_out, blk, val)
                        && !LIVE_BIT(phi_def, to, val)) {
                        LIVE_BIT(live_out, blk, val) = 1;
                        changed = 1;
                    }
                for (pred = 0; pred != preds[to].count; pred++) {
                    if (preds[to].at[pred] != blk)
                        continue;
                    for (phi_at = 0; phi_at != nphis; phi_at++) {
                        const Phi *phi = &phis[phi_at];

                        if (!phi->live || phi->block != to || phi->in[pred] < 0
                            || LIVE_BIT(live_out, blk, phi->in[pred]))
                            continue;
                        LIVE_BIT(live_out, blk, phi->in[pred]) = 1;
                        changed = 1;
                    }
                }
            }
            for (val = 0; val != nvals; val++) {
                int in = LIVE_BIT(block_gen, blk, val)
                         || (LIVE_BIT(live_out, blk, val)
                             && !LIVE_BIT(block_kill, blk, val));

                if (in && !LIVE_BIT(live_in, blk, val)) {
                    LIVE_BIT(live_in, blk, val) = 1;
                    changed = 1;
                }
            }
        }
    } while (changed);

    /* The intervals: each value's making, its reads, and every block it is
     * live into or out of, end to end. */
    for (val = 0; val != nvals; val++) {
        vals[val].def = vals[val].def < 0 ? 0 : vals[val].def;
        vals[val].last = vals[val].def;
    }
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param)
            widen(locals[local].entry_val, 0);
    for (blk = 0; blk != nblocks; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;
        int first = blocks[blk].first, last = block_last[blk];

        if (last < first)
            last = first;
        for (val = 0; val != nvals; val++) {
            if (LIVE_BIT(live_in, blk, val))
                widen(val, first);
            if (LIVE_BIT(live_out, blk, val))
                widen(val, last);
        }
        for (at = first; at != end; at++) {
            const Ins *insn = &insns[at];

            for (operand = 0; operand != insn->nin; operand++)
                if (insn->in[operand].val >= 0)
                    widen(insn->in[operand].val, at);
            if (insn->res >= 0)
                widen(insn->res, at);
            if (insn->op == I_SET && insn->target >= 0)
                widen(insn->target, at);
        }
    }
    for (phi_at = 0; phi_at != nphis; phi_at++)
        if (phis[phi_at].live)
            widen(phis[phi_at].val, blocks[phis[phi_at].block].first);

    free(block_gen);
    free(block_kill);
    free(phi_def);
    block_gen = block_kill = phi_def = NULL;
}

/* ------------------------------------------------------------------ */
/* generating code from it                                             */

/* Where gen_data put a string the first time and where it is now, so that
 * an address inside one can be moved with it. */
typedef struct {
    int old_at, len, new_at;
} Moved;

static Moved *moved;
static int    nmoved, moved_cap;

static int moved_at(int at)
{
    int idx;

    for (idx = 0; idx != nmoved; idx++)
        if (at >= moved[idx].old_at && at <= moved[idx].old_at + moved[idx].len)
            return at - moved[idx].old_at + moved[idx].new_at;

    return at;
}

/* A constant onto the classic backend's stack, made again. */
static void load_constant(const Ent *ent)
{
    const Value *attr = &ent->attr;

    if (attr->kind == VAL_WIDE) {
        if (type_float(attr->type)) {
            uint32_t bits = (uint32_t) ent->wide;
            float fval;

            memcpy(&fval, &bits, sizeof fval);
            vpush_const_float(fval);
        } else if (type_eight(attr->type)) {
            vpush_const_wide((uint32_t) ent->wide,
                             (uint32_t) (ent->wide >> 32), attr->type);
        } else {
            vpush_const_long((long) (int32_t) ent->wide, attr->type);
        }
    } else if (attr->kind == VAL_BSS) {
        vpush_bss(attr->val, attr->type);
    } else if (attr->kind == VAL_ADDR) {
        vpush_const(moved_at(attr->val), attr->type);
        vset_addr();
    } else {
        vpush_const(attr->val, attr->type);
    }
}

/* An operand onto the classic backend's stack, as it was on the builder's. */
static void load(const Ent *ent)
{
    const Value *attr = &ent->attr;

    if (ent->val == S_VOID) {
        fail = "internal: a void operand";
        return;
    }
    if (ent->val == S_CONST) {
        load_constant(ent);
    } else {
        Type stored = vals[ent->val].type;

        vpush_local(vals[ent->val].slot, stored);

        /* Read where the first pass had it at another type: the same
         * address as another pointer is a relabel, and anything else --
         * a char read where the first pass had it promoted, say -- is a
         * conversion. */
        if (attr->type != stored) {
            if (type_pointer(attr->type) && type_pointer(stored))
                vset_type(attr->type, attr->ext);
            else
                vconvert(attr->type);
        }
    }
    vset_ext(attr->ext);
    vset_quals(attr->quals);
    if (attr->bits)
        vset_bits(attr->bits);
}

/* The call an instruction came from, made again with the same arguments --
 * but a string's address moved to where the string is now. */
static void call(const Ins *insn)
{
    const GenRec *rec = insn->rec;
    const long *arg = rec->arg;

    switch (insn->op) {
#define ARG(at, type) ((type) arg[at])
    case GL_vpush_const:        vpush_const(ARG(0, int), ARG(1, Type)); break;
    case GL_vpush_bss:          vpush_bss(ARG(0, int), ARG(1, Type)); break;
    case GL_vpush_const_long:   vpush_const_long(ARG(0, long), ARG(1, Type)); break;
    case GL_vpush_const_wide:
        vpush_const_wide(ARG(0, uint32_t), ARG(1, uint32_t), ARG(2, Type));
        break;
    case GL_vpush_const_float: {
        float fval;

        memcpy(&fval, &arg[0], sizeof fval);
        vpush_const_float(fval);
        break;
    }
    case GL_vconvert:           vconvert(ARG(0, Type)); break;
    case GL_vpush_local:        vpush_local(ARG(0, int), ARG(1, Type)); break;
    case GL_vstore_local:       vstore_local(ARG(0, int), ARG(1, Type)); break;
    case GL_vapply:             vapply(ARG(0, unsigned char), ARG(1, Type)); break;
    case GL_vaddr_local:        vaddr_local(ARG(0, int), ARG(1, Type)); break;
    case GL_vaddr_array:        vaddr_array(ARG(0, int), ARG(1, Type)); break;
    case GL_vderef:             vderef(); break;
    case GL_vmember:
        vmember(ARG(0, int), ARG(1, Type), ARG(2, int), ARG(3, int));
        break;
    case GL_vstore_indirect:    vstore_indirect(); break;
    case GL_vneg:               vneg(); break;
    case GL_vnot:               vnot(); break;
    case GL_vtruth:             vtruth(ARG(0, int)); break;
    case GL_vcast:              vcast(ARG(0, Type), ARG(1, int), ARG(2, int)); break;
    case GL_gen_call:
        gen_call(ARG(0, int), ARG(1, int), ARG(2, int), ARG(3, int));
        break;
    case GL_gen_call_indirect:  gen_call_indirect(ARG(0, int)); break;
    case GL_vpush_function:     vpush_function(ARG(0, int)); break;
    case GL_vpush_global_addr:  vpush_global_addr(ARG(0, int)); break;
    case GL_vprefix_local:
        vprefix_local(ARG(0, int), ARG(1, Type), ARG(2, int), ARG(3, int));
        break;
    case GL_vpostfix_local:
        vpostfix_local(ARG(0, int), ARG(1, Type), ARG(2, int), ARG(3, int));
        break;
    case GL_vprefix_indirect:   vprefix_indirect(ARG(0, int)); break;
    case GL_vpostfix_indirect:  vpostfix_indirect(ARG(0, int)); break;
    case GL_gen_stmt_end:       gen_stmt_end(); break;
    case GL_gen_value_end:      gen_value_end(); break;
    case GL_gen_zero_array:
        gen_zero_array(ARG(0, int), ARG(1, int), ARG(2, int));
        break;
    case GL_gen_copy_to_array:
        gen_copy_to_array(ARG(0, int), ARG(1, int), moved_at(ARG(2, int)),
                          ARG(3, int));
        break;
    case GL_gen_data: {
        int at = gen_data(gl_kept(arg[0]), ARG(1, int));

        GROW(moved, nmoved, moved_cap);
        moved[nmoved].old_at = (int) rec->ret;
        moved[nmoved].len = ARG(1, int);
        moved[nmoved].new_at = at;
        nmoved++;
        break;
    }
    case GL_gen_return:
        gen_return(ARG(0, int), (const char *) (intptr_t) arg[1]);
        break;
    case GL_gen_local:          (void) gen_local(ARG(0, int)); break;
    case GL_gen_local_far:      (void) gen_local_far(ARG(0, int)); break;
    case GL_gen_local_array:    (void) gen_local_array(); break;
    case GL_gen_local_array_size:
        gen_local_array_size(ARG(0, int), ARG(1, int));
        break;
    case GL_gen_iy_claim:
        gen_iy_claim(ARG(0, int), ARG(1, Type), ARG(2, int));
        break;
    case GL_gen_iy_param:       gen_iy_param(ARG(0, int)); break;
    case GL_gen_iy_take:        gen_iy_take(ARG(0, int)); break;
    case GL_gen_func_begin:
        gen_func_begin(ARG(0, int), ARG(1, int), ARG(2, Type));
        break;
#undef ARG
    default:
        fail = gl_names[insn->op];
        break;
    }
}

/* Where each block begins in the code now, once it has; and the jumps to a
 * block not reached yet, to be filled in when it is. */
static int *block_now;

typedef struct {
    int hole, block;
} Pending;

static Pending *pending;
static int      npending, pending_cap;

static void jump_forward(int hole, int block)
{
    if (!hole)
        return;
    GROW(pending, npending, pending_cap);
    pending[npending].hole = hole;
    pending[npending].block = block;
    npending++;
}

static void emit_block_start(int block)
{
    int at;

    for (at = 0; at != npending; at++)
        if (pending[at].block == block && pending[at].hole) {
            gen_label(pending[at].hole);
            pending[at].hole = 0;
        }
    block_now[block] = gen_here();
}

/* A value just made, on top of the classic stack, into its slot -- or,
 * when nothing reads it, dropped. */
static void keep(int val)
{
    if (!vals[val].used) {
        gen_discard();
        return;
    }
    vstore_local(vals[val].slot, vals[val].type);
    vdrop();
}

/* A slot for each value, reused once the value in it is dead: planned
 * before anything is emitted, so that a function whose values would not all
 * be in reach of (ix+d) goes to the classic backend instead. */
static int *slot_of;            /* by value: which slot */
static int *slot_size, nslots;

/* A web whose slot is decided already, by its root: a parameter's own, or
 * the local in IY (IY_SLOT, made in give_slots); 0 for none. */
static int *fixed_slot;
static int  iy_web = -1;        /* the web in IY, or -1 */

#define IY_SLOT 1

static int plan_slots(void)
{
    int *free_at, val, slot, bytes = 0;

    slot_of = realloc(slot_of, ((size_t) nvals + 1) * sizeof *slot_of);
    slot_size = realloc(slot_size, ((size_t) nvals + 1) * sizeof *slot_size);
    free_at = calloc((size_t) nvals + 1, sizeof *free_at);
    if (!slot_of || !slot_size || !free_at)
        acc_error("out of memory for the SSA form");
    nslots = 0;

    /* A slot a web: each of its values in it, for the web's whole span. */
    for (val = 0; val != nvals; val++) {
        int size = type_bytes(vals[val].type, 0), first, last, member;

        slot_of[val] = -1;
        if (vals[val].fwd || vals[val].reg != HOME_SLOT || web_root(val) != val
            || (fixed_slot && fixed_slot[val]))
            continue;                   /* on the stack, in a register, in
                                         * its root's slot, or where it is
                                         * put already */
        first = vals[val].def;
        last = vals[val].last;
        for (member = web_next[val]; member >= 0; member = web_next[member]) {
            if (vals[member].def < first)
                first = vals[member].def;
            if (vals[member].last > last)
                last = vals[member].last;
        }
        if (size < ACC_INT_SIZE)
            size = ACC_INT_SIZE;
        for (slot = 0; slot != nslots; slot++)
            if (free_at[slot] < first && slot_size[slot] == size)
                break;
        if (slot == nslots) {
            slot_size[slot] = size;
            bytes += size;
            nslots++;
        }
        slot_of[val] = slot;
        free_at[slot] = last;
    }
    for (val = 0; val != nvals; val++)
        if (!vals[val].fwd && vals[val].reg == HOME_SLOT
            && !(fixed_slot && fixed_slot[web_root(val)]))
            slot_of[val] = slot_of[web_root(val)];
    free(free_at);

    /* The first pass's locals are all taken by now: what is left in reach
     * is what gen_local_fits says. */
    if (bytes && !gen_local_fits(bytes)) {
        fail = "more values than the frame can reach";
        return 0;
    }

    return 1;
}

/* The planned slots, taken from the frame. */
static void give_slots(void)
{
    int *slot_at = malloc(((size_t) nslots + 1) * sizeof *slot_at);
    int val, slot;

    if (!slot_at)
        acc_error("out of memory for the SSA form");
    for (slot = 0; slot != nslots; slot++)
        slot_at[slot] = gen_local(slot_size[slot]);
    for (val = 0; val != nvals; val++)
        if (slot_of[val] >= 0)
            vals[val].slot = slot_at[slot_of[val]];
    free(slot_at);

    /* The web in IY: a slot of its own that IY stands for, as the local
     * the first pass would have put there. */
    if (iy_web >= 0) {
        int at = gen_local(ACC_INT_SIZE);

        gen_iy_take(at);
        fixed_slot[iy_web] = at;
    }
    for (val = 0; fixed_slot && val != nvals; val++)
        if (!vals[val].fwd && vals[val].reg == HOME_SLOT
            && fixed_slot[web_root(val)])
            vals[val].slot = fixed_slot[web_root(val)];
}

/* A block's live phis, which say whether an edge into it needs copies. */
static int has_phis(int blk)
{
    int phi;

    for (phi = 0; phi != nphis; phi++)
        if (phis[phi].live && phis[phi].block == blk)
            return 1;

    return 0;
}

/* The copies an edge makes into the phis of the block it goes to: every
 * value the edge brings loaded first and held -- in a register, or where
 * the backend puts one when it runs out -- and only then stored, the last
 * first, so that a phi read by another's copy is read before it changes. */
static void edge_copies(int from, int to)
{
    int nstored = 0, stored[MAX_LOCALS * 4], phi, pred;

    for (pred = 0; pred != preds[to].count; pred++) {
        if (preds[to].at[pred] != from)
            continue;
        for (phi = 0; phi != nphis; phi++) {
            const Phi *join = &phis[phi];
            Ent ent;

            if (!join->live || join->block != to || join->in[pred] < 0
                || join->in[pred] == join->val
                || nstored == (int) (sizeof stored / sizeof stored[0]))
                continue;
            memset(&ent, 0, sizeof ent);
            ent.val = join->in[pred];
            ent.attr.kind = VAL_LOCAL;
            ent.attr.type = vals[ent.val].type;
            load(&ent);
            (void) force_reg(vsp - 1);  /* held now, not read later */
            stored[nstored++] = join->val;
        }
        break;                          /* one edge's worth: the first */
    }
    while (nstored--) {
        int val = stored[nstored];

        vstore_local(vals[val].slot, vals[val].type);
        vdrop();
    }
}

/* Jumps to copies made where nothing falls into them: an edge that
 * branches into a block with phis goes there first. */
typedef struct {
    int hole, from, to;
} Trampoline;

static Trampoline *trampolines;
static int         ntrampolines, trampolines_cap;

static int  regs_on(void);
static void edge_moves(int from, int to);

/* The copies of the trampolines waiting, each then a jump on to its
 * block: made where the code before cannot fall into them. */
static void emit_trampolines(void)
{
    int tramp;

    for (tramp = 0; tramp != ntrampolines && !fail; tramp++) {
        int to = trampolines[tramp].to;

        gen_label(trampolines[tramp].hole);
        if (regs_on())
            edge_moves(trampolines[tramp].from, to);
        else
            edge_copies(trampolines[tramp].from, to);
        if (block_now[to] >= 0)
            gen_jump_to(block_now[to]);
        else
            jump_forward(gen_jump(), to);
    }
    ntrampolines = 0;
}

static void emit_branch(const Ins *insn, int blk)
{
    load(&insn->in[0]);
    if (insn->target < 0) {
        vdrop();
        return;
    }
    if (has_phis(insn->target)) {
        if (insn->sense)
            vtruth(TK_EQ);
        GROW(trampolines, ntrampolines, trampolines_cap);
        trampolines[ntrampolines].hole = gen_jump_if_false();
        trampolines[ntrampolines].from = blk;
        trampolines[ntrampolines].to = insn->target;
        ntrampolines++;
        return;
    }
    /* The truth turned over is `== 0`: vnot is ~, not !. */
    if (block_now[insn->target] >= 0) {
        if (!insn->sense)
            vtruth(TK_EQ);
        gen_jump_if_true_to(block_now[insn->target]);
    } else {
        if (insn->sense)
            vtruth(TK_EQ);
        jump_forward(gen_jump_if_false(), insn->target);
    }
}

/* Whether the first pass put a local in IY that stays in memory: then IY
 * is that local's, and no web's. */
static int iy_taken_already(void)
{
    int at;

    for (at = 1; at != ninsns; at++)
        if (insns[at].op == I_FRAME && !frame_of_value(&insns[at])
            && (insns[at].rec->op == GL_gen_iy_claim
                || insns[at].rec->op == GL_gen_iy_param
                || insns[at].rec->op == GL_gen_iy_take))
            return 1;

    return 0;
}

/* Whether a frame call is about a local that is values now, and so is not
 * made: the local in IY is one of them. */
static int frame_of_value(const Ins *insn)
{
    int local;

    switch (insn->rec->op) {
    case GL_gen_iy_claim: case GL_gen_iy_param: case GL_gen_iy_take:
        local = local_of((int) insn->rec->arg[0]);
        return local >= 0 && locals[local].ok;
    }

    return 0;
}

static void emit_insn(const Ins *insn, int blk, int at)
{
    int operand;

    switch (insn->op) {
    case I_FRAME:
    case GL_vdrop:
        break;
    case I_SET:
        load(&insn->in[0]);
        if (insn->target >= 0) {
            vconvert(vals[insn->target].type);
            vstore_local(vals[insn->target].slot, vals[insn->target].type);
        }
        vdrop();
        break;
    case I_CONV:
        load(&insn->in[0]);

        /* An address the link fills in is cut down in a register, as the
         * store it stands for would: vconvert refuses it as a constant. */
        if (val_pending((vsp - 1)->kind)
            && type_size(insn->local_type) < ACC_INT_SIZE)
            force_reg(vsp - 1);
        vconvert(insn->local_type);
        keep(insn->res);
        break;
    case I_STEP:
        load(&insn->in[0]);
        vpush_const(1, TY_INT);
        vapply((unsigned char) insn->step_op, type_narrow(insn->local_type));
        vconvert(insn->local_type);
        keep(insn->res);
        break;
    case I_JMP:
        if (insn->target < 0)
            break;                      /* never landed: nothing jumps */
        edge_copies(blk, insn->target);
        if (block_now[insn->target] >= 0)
            gen_jump_to(block_now[insn->target]);
        else if (insn->target != blk + 1 || block_last[blk] != at)
            jump_forward(gen_jump(), insn->target);
        break;
    case I_BR:
        emit_branch(insn, blk);
        break;
    case GL_gen_switch_load:
        gen_switch_load((int) insn->rec->arg[0], (Type) insn->rec->arg[1]);
        break;
    case GL_gen_switch_case:
        gen_switch_case(insn->rec->arg[0], (uint32_t) insn->rec->arg[1],
                        (Type) insn->rec->arg[2], block_now[insn->target],
                        (int) insn->rec->arg[4]);
        break;
    default:
        for (operand = 0; operand != insn->nin; operand++)
            load(&insn->in[operand]);
        call(insn);
        if (insn->res >= 0)
            keep(insn->res);
        else if (pushes(insn->op))
            vdrop();                    /* a void, or a constant made again */
        break;
    }
}

static void emit(void)
{
    int at, blk, local;

    block_now = malloc((size_t) nblocks * sizeof *block_now);
    if (!block_now)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != nblocks; at++)
        block_now[at] = -1;
    npending = 0;
    nmoved = 0;
    ntrampolines = 0;

    /* The prologue, and the frame as the first pass laid it out -- but
     * for the local in IY, if it is values now. */
    call(&insns[0]);
    for (at = 1; at != ninsns; at++)
        if (insns[at].op == I_FRAME && !frame_of_value(&insns[at])) {
            Ins frame_call = insns[at];

            frame_call.op = frame_call.rec->op;
            call(&frame_call);
        }
    give_slots();

    /* Each parameter that is values now, from its slot into its first. */
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param) {
            int val = locals[local].entry_val;

            vpush_local(locals[local].offset, locals[local].type);
            vstore_local(vals[val].slot, vals[val].type);
            vdrop();
        }

    for (blk = 0; blk != nblocks && !fail; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;
        int falls = 1;

        if (blk)
            emit_block_start(blk);
        for (at = blocks[blk].first; at != end && !fail; at++) {
            if (at == 0)
                continue;               /* the prologue, made above */
            emit_insn(&insns[at], blk, at);
            if (at == end - 1 && (insns[at].op == I_JMP
                                  || insns[at].op == GL_gen_return))
                falls = 0;
        }
        if (falls && blk + 1 < nblocks)
            edge_copies(blk, blk + 1);
        if (!falls)
            emit_trampolines();
    }

    /* The last block falls into the epilogue: over the rest. */
    if (ntrampolines) {
        int over = gen_jump();

        emit_trampolines();
        gen_label(over);
    }
    free(block_now);
    block_now = NULL;
}

/* ------------------------------------------------------------------ */
/* values in registers                                                 */

/* With OPTACC_REGS, code is made the same way -- each instruction a call
 * of gen.h again -- but a value no longer goes through a frame slot on its
 * way from the instruction that makes it to the ones that read it.
 *
 * - A value read once, by an instruction in its own block, in the order
 *   the classic stack would have it, is left on that stack: the classic
 *   backend sees the expression it saw the first time (find_forwarded).
 * - Any other int-sized value lives in BC or DE for the whole of its
 *   interval when linear scan finds one free, and in its frame slot when
 *   not (plan_homes). HL stays the classic backend's own.
 * - While a register holds a value, the value sits at the bottom of the
 *   classic stack as that register -- a pin -- so that the classic backend
 *   counts the register as taken: it moves the value, or spills it around
 *   a call, as it would one of its own. Whenever nothing but pins is left
 *   on the stack, each goes back to its register (pins_restore). */

static int regs_on(void)
{
    return getenv("OPTACC_REGS") != NULL;
}

/* The instruction each value is the result of, or -1: a phi, a parameter,
 * the answer of a ?: or of && and ||. */
static int *def_at;

/* Whether a value is narrower than an int and made by arithmetic: the
 * classic backend leaves one of those in a register at its type but not cut
 * to its width -- `c + 100` of a char is 200 there -- and cuts it where it
 * stores it. The first pass stored it, to the local it was assigned to, and
 * read it back; left on the stack it would be read uncut. A narrow value
 * read from memory was widened as it was loaded. */
static int narrow_unwidened(int val)
{
    int op = insns[def_at[val]].op;

    return type_size(vals[val].type) < ACC_INT_SIZE
           && op != GL_vpush_local && op != GL_vderef && op != GL_vmember
           && op != GL_gen_call && op != GL_gen_call_indirect;
}

/* Which values are left on the classic stack: read once, in the block that
 * makes them, and in stack order -- checked by walking each block with a
 * stack of its own, and a value that is not where the classic stack would
 * have it is taken out and the walk made again. */
static void find_forwarded(void)
{
    int *uses = calloc((size_t) nvals + 1, sizeof *uses);
    int *use_at = calloc((size_t) nvals + 1, sizeof *use_at);
    int *sim = malloc(((size_t) nvals + 1) * sizeof *sim);
    int val, at, operand, phi, pred, changed;

    def_at = realloc(def_at, ((size_t) nvals + 1) * sizeof *def_at);
    if (!uses || !use_at || !sim || !def_at)
        acc_error("out of memory for the SSA form");
    for (val = 0; val != nvals; val++)
        def_at[val] = -1;
    for (at = 0; at != ninsns; at++) {
        if (insns[at].res >= 0)
            def_at[insns[at].res] = at;
        for (operand = 0; operand != insns[at].nin; operand++) {
            val = insns[at].in[operand].val;
            if (val >= 0) {
                uses[val]++;
                use_at[val] = at;
            }
        }
    }
    for (phi = 0; phi != nphis; phi++)
        for (pred = 0; phis[phi].live && pred != preds[phis[phi].block].count;
             pred++)
            if (phis[phi].in[pred] >= 0)
                uses[phis[phi].in[pred]] += 2;
    for (val = 0; val != nvals; val++)
        vals[val].fwd = def_at[val] >= 0 && uses[val] == 1
                        && use_at[val] > def_at[val]
                        && insns[use_at[val]].block == insns[def_at[val]].block
                        && !narrow_unwidened(val);

    do {
        int blk;

        changed = 0;
        for (blk = 0; blk != nblocks && !changed; blk++) {
            int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;
            int nsim = 0;

            for (at = blocks[blk].first; at != end && !changed; at++) {
                const Ins *insn = &insns[at];
                int want = 0, rank = 0;

                for (operand = 0; operand != insn->nin; operand++) {
                    val = insn->in[operand].val;
                    if (val >= 0 && vals[val].fwd)
                        want++;
                }
                for (operand = 0; operand != insn->nin; operand++) {
                    val = insn->in[operand].val;
                    if (val < 0 || !vals[val].fwd)
                        continue;
                    if (want > nsim || sim[nsim - want + rank] != val)
                        changed = 1;
                    rank++;
                }
                if (changed) {
                    for (operand = 0; operand != insn->nin; operand++)
                        if (insn->in[operand].val >= 0)
                            vals[insn->in[operand].val].fwd = 0;
                    break;
                }
                nsim -= want;
                if (insn->res >= 0 && vals[insn->res].fwd)
                    sim[nsim++] = insn->res;
            }
            if (!changed && nsim) {
                while (nsim)
                    vals[sim[--nsim]].fwd = 0;
                changed = 1;
            }
        }
    } while (changed);
    free(uses);
    free(use_at);
    free(sim);
}

/* Whether a value can live in a register: a scalar an int or narrower,
 * read nowhere as a bit-field. */
static int reg_eligible(int val)
{
    Type type = vals[val].type;

    if (vals[val].fwd || !vals[val].used || type == TY_VOID
        || type_is_struct(type) || type_wide(type) || type_float(type)
        || type_size(type) > ACC_INT_SIZE)
        return 0;
    if (def_at[val] >= 0 && insns[def_at[val]].rec->top.bits)
        return 0;

    return 1;
}

#define NHOMES 2

static const int home_regs[NHOMES] = { R_BC, R_DE };

/* How many of them values may live in: OPTACC_HOMES, 1 or 2. Every one
 * taken is one fewer for the classic backend's own work. */
static int homes_wanted(void)
{
    const char *homes = getenv("OPTACC_HOMES");

    return homes && *homes == '2' ? 2 : homes && *homes == '0' ? 0 : 1;
}

/* Which values are live at once, and so cannot share a home: a value
 * clashes with every value live just after the instruction that makes it
 * -- after, so that one read last there can give its home to the result --
 * and the phis and parameters, made at the top of a block, with every value
 * live there. Found walking each block back from what is live out of it,
 * which also finds each operand that is the last read of its value. */
static unsigned char *clash_bits, *live_top;
static size_t clash_stride;

static int clashes(int one, int two)
{
    return clash_bits[(size_t) one * clash_stride + (size_t) two / 8]
           & (1 << (two % 8));
}

static void clash(int one, int two)
{
    if (one == two)
        return;
    clash_bits[(size_t) one * clash_stride + (size_t) two / 8]
        |= (unsigned char) (1 << (two % 8));
    clash_bits[(size_t) two * clash_stride + (size_t) one / 8]
        |= (unsigned char) (1 << (one % 8));
}

/* Each value made at the top of `blk` clashes with what is live there and
 * with the others made there. */
static void clash_at_top(int blk, const unsigned char *live)
{
    int made[MAX_LOCALS * 4], nmade = 0, phi, local, val, at;

    for (phi = 0; phi != nphis; phi++)
        if (phis[phi].live && phis[phi].block == blk
            && nmade != (int) (sizeof made / sizeof made[0]))
            made[nmade++] = phis[phi].val;
    for (local = 0; blk == 0 && local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param
            && nmade != (int) (sizeof made / sizeof made[0]))
            made[nmade++] = locals[local].entry_val;
    for (at = 0; at != nmade; at++) {
        for (val = 0; val != nvals; val++)
            if (live[val])
                clash(made[at], val);
        for (val = 0; val != at; val++)
            clash(made[at], made[val]);
    }
}

static void find_clashes(void)
{
    unsigned char *live = malloc((size_t) nvals + 1);
    int blk;

    clash_stride = ((size_t) nvals + 7) / 8 + 1;
    clash_bits = calloc((size_t) nvals * clash_stride + 1, 1);
    live_top = calloc((size_t) nblocks * (size_t) nvals + 1, 1);
    if (!live || !clash_bits || !live_top)
        acc_error("out of memory for the SSA form");
    for (blk = 0; blk != nblocks; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns, at;

        memcpy(live, &LIVE_BIT(live_out, blk, 0), (size_t) nvals);
        for (at = end - 1; at >= blocks[blk].first; at--) {
            Ins *insn = &insns[at];
            int made = insn->res >= 0 ? insn->res
                       : insn->op == I_SET ? insn->target : -1, val, operand;

            insn->kills = 0;
            if (made >= 0) {
                live[made] = 0;
                for (val = 0; val != nvals; val++)
                    if (live[val])
                        clash(made, val);
            }
            for (operand = insn->nin - 1; operand >= 0; operand--) {
                val = insn->in[operand].val;
                if (val < 0 || live[val])
                    continue;
                insn->kills |= (unsigned char) (1 << operand);
                live[val] = 1;
            }
        }
        clash_at_top(blk, live);
        memcpy(&LIVE_BIT(live_top, blk, 0), live, (size_t) nvals);
    }
    free(live);
}

/* Webs: a phi and the values that come into it joined, where their
 * intervals allow, so that they have one home and the copies between them
 * are no copies. A union-find, and each web's values in a list from its
 * root through web_next. */
static int *web_parent, *web_tail;
static long *web_weight;

static int web_root(int val)
{
    while (web_parent[val] != val)
        val = web_parent[val] = web_parent[web_parent[val]];

    return val;
}

static int webs_clash(int one, int two)
{
    int left, right;

    for (left = one; left >= 0; left = web_next[left])
        for (right = two; right >= 0; right = web_next[right])
            if (clashes(left, right))
                return 1;

    return 0;
}

/* How deep in loops each block is: a jump to a block that dominates the
 * one it is made from closes a loop, whose body is what reaches the jump
 * without going through the block it goes to. */
static int *loop_depth;

static int dominates(int over, int blk)
{
    while (blk != over && idom[blk] >= 0 && idom[blk] != blk)
        blk = idom[blk];

    return blk == over;
}

static void find_loops(void)
{
    int *work = malloc(((size_t) nblocks + 1) * sizeof *work);
    unsigned char *in_loop = malloc((size_t) nblocks + 1);
    int blk, succ;

    loop_depth = realloc(loop_depth, ((size_t) nblocks + 1) * sizeof *loop_depth);
    if (!work || !in_loop || !loop_depth)
        acc_error("out of memory for the SSA form");
    for (blk = 0; blk != nblocks; blk++)
        loop_depth[blk] = 0;
    for (blk = 0; blk != nblocks; blk++) {
        if (rpo_num[blk] < 0)
            continue;
        for (succ = 0; succ != succs[blk].count; succ++) {
            int head = succs[blk].at[succ], nwork = 0, at;

            if (rpo_num[head] < 0 || !dominates(head, blk))
                continue;
            memset(in_loop, 0, (size_t) nblocks);
            in_loop[head] = 1;
            if (!in_loop[blk]) {
                in_loop[blk] = 1;
                work[nwork++] = blk;
            }
            while (nwork) {
                int body = work[--nwork], pred;

                for (pred = 0; pred != preds[body].count; pred++) {
                    int from = preds[body].at[pred];

                    if (!in_loop[from] && rpo_num[from] >= 0) {
                        in_loop[from] = 1;
                        work[nwork++] = from;
                    }
                }
            }
            for (at = 0; at != nblocks; at++)
                loop_depth[at] += in_loop[at];
        }
    }
    free(work);
    free(in_loop);
}

static void coalesce(void)
{
    size_t size = ((size_t) nvals + 1) * sizeof *web_parent;
    int val, phi, pred, at, operand;

    web_parent = realloc(web_parent, size);
    web_next = realloc(web_next, size);
    web_tail = realloc(web_tail, size);
    web_weight = realloc(web_weight, ((size_t) nvals + 1) * sizeof *web_weight);
    if (!web_parent || !web_next || !web_tail || !web_weight)
        acc_error("out of memory for the SSA form");
    for (val = 0; val != nvals; val++) {
        web_parent[val] = val;
        web_next[val] = -1;
        web_tail[val] = val;
        web_weight[val] = 0;
    }
    if (!regs_on())
        return;

    for (phi = 0; phi != nphis; phi++) {
        const Phi *join = &phis[phi];

        if (!join->live)
            continue;
        for (pred = 0; pred != preds[join->block].count; pred++) {
            int source = join->in[pred], into, from;

            if (source < 0 || vals[source].fwd
                || vals[source].type != vals[join->val].type)
                continue;
            into = web_root(join->val);
            from = web_root(source);
            if (into == from || webs_clash(into, from))
                continue;
            web_parent[from] = into;
            web_next[web_tail[into]] = from;
            web_tail[into] = web_tail[from];
        }
    }

    /* Each read, and each making, eight times over for each loop around
     * it: what a register saves is paid where the value is used. */
    find_loops();
    for (at = 0; at != ninsns; at++) {
        int depth = loop_depth[insns[at].block];
        long weight = 1L << (3 * (depth > 5 ? 5 : depth));

        for (operand = 0; operand != insns[at].nin; operand++)
            if (insns[at].in[operand].val >= 0)
                web_weight[web_root(insns[at].in[operand].val)] += weight;
        if (insns[at].res >= 0)
            web_weight[web_root(insns[at].res)] += weight;
    }
}

static int by_weight(const void *left, const void *right)
{
    int lval = *(const int *) left, rval = *(const int *) right;

    if (web_weight[lval] != web_weight[rval])
        return web_weight[lval] > web_weight[rval] ? -1 : 1;

    return lval < rval ? -1 : lval > rval;
}

static int iy_taken_already(void);

/* Webs whose slots are decided before the rest: the heaviest web left in
 * the frame that IY can hold goes in IY, if the first pass left IY free;
 * and a web holding a parameter's first value keeps to the parameter's
 * own slot, which nothing else reads now. */
static void plan_fixed(const int *order, int nroots)
{
    int at, local;

    fixed_slot = realloc(fixed_slot, ((size_t) nvals + 1) * sizeof *fixed_slot);
    if (!fixed_slot)
        acc_error("out of memory for the SSA form");
    memset(fixed_slot, 0, ((size_t) nvals + 1) * sizeof *fixed_slot);
    iy_web = -1;
    for (at = 0; at != nroots && !iy_taken_already() && getenv("OPTACC_IY"); at++) {
        int root = order[at], member, fits = 1;

        for (member = root; member >= 0; member = web_next[member])
            if (vals[member].fwd || vals[member].reg != HOME_SLOT
                || !reg_eligible(member) || !gen_iy_can(vals[member].type))
                fits = 0;

        /* Nor a parameter out of (ix+d)'s reach: the classic backend
         * reaches one through IY, which it cannot then load into IY. */
        for (local = 0; local != nlocals; local++)
            if (locals[local].ok && locals[local].is_param
                && web_root(locals[local].entry_val) == root
                && !disp_fits(locals[local].offset))
                fits = 0;
        if (!fits)
            continue;
        iy_web = root;
        fixed_slot[root] = IY_SLOT;     /* made in give_slots */
        break;
    }
    for (local = 0; local != nlocals; local++) {
        int root;

        if (!locals[local].ok || !locals[local].is_param)
            continue;
        root = web_root(locals[local].entry_val);
        if (!fixed_slot[root] && vals[root].reg == HOME_SLOT && !vals[root].fwd)
            fixed_slot[root] = locals[local].offset;
    }
}

/* The webs, heaviest first, each given a register when every value in it
 * can live in one and none of them clashes with a value the register was
 * given already; the rest keep to their frame slots. */
static void plan_homes(void)
{
    int *order = malloc(((size_t) nvals + 1) * sizeof *order);
    int *given[NHOMES], ngiven[NHOMES], nroots = 0, at, reg, operand;

    for (reg = 0; reg != NHOMES; reg++) {
        given[reg] = malloc(((size_t) nvals + 1) * sizeof *given[reg]);
        ngiven[reg] = 0;
        if (!given[reg])
            acc_error("out of memory for the SSA form");
    }
    if (!order)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != nvals; at++)
        vals[at].reg = HOME_SLOT;

    /* Read as a bit-field anywhere: in its slot, where the classic backend
     * reads bit-fields from. */
    for (at = 0; at != ninsns; at++)
        for (operand = 0; operand != insns[at].nin; operand++)
            if (insns[at].in[operand].val >= 0 && insns[at].in[operand].attr.bits)
                vals[insns[at].in[operand].val].reg = -2;

    for (at = 0; at != nvals; at++)
        if (web_root(at) == at)
            order[nroots++] = at;
    qsort(order, (size_t) nroots, sizeof *order, by_weight);

    for (at = 0; at != nroots; at++) {
        int root = order[at], homes = homes_wanted(), member, fits = 1;

        for (member = root; member >= 0 && fits; member = web_next[member]) {
            int insn_at;

            if (vals[member].reg == -2 || !reg_eligible(member))
                fits = 0;

            /* A switch's tests load DE: a value across them is in BC. */
            for (insn_at = vals[member].def;
                 fits && insn_at <= vals[member].last; insn_at++)
                if (insns[insn_at].op == GL_gen_switch_case)
                    homes = 1;
        }
        for (reg = 0; reg != homes && fits; reg++) {
            int clash = 0, other;

            for (member = root; member >= 0 && !clash; member = web_next[member])
                for (other = 0; other != ngiven[reg] && !clash; other++)
                    clash = clashes(member, given[reg][other]);
            if (clash)
                continue;
            for (member = root; member >= 0; member = web_next[member]) {
                vals[member].reg = home_regs[reg];
                given[reg][ngiven[reg]++] = member;
            }
            fits = 0;                   /* placed */
        }
    }
    for (at = 0; at != nvals; at++)
        if (vals[at].reg == -2)
            vals[at].reg = HOME_SLOT;
    plan_fixed(order, nroots);
    for (reg = 0; reg != NHOMES; reg++)
        free(given[reg]);
    free(order);
}

/* The pins: the bottom npins entries of the classic stack, and the value
 * each is. */
#define MAX_PINS 8

static int pin_val[MAX_PINS], npins;

static void stack_insert(int index, const Value *entry)
{
    if (vtop + 1 >= VSTACK_MAX) {
        fail = "internal: the classic stack is full";
        return;
    }
    memmove(&vstack[index + 1], &vstack[index],
            (size_t) (vtop - index) * sizeof *vstack);
    vstack[index] = *entry;
    vtop++;
    vsp++;
}

static Value stack_remove(int index)
{
    Value entry = vstack[index];

    memmove(&vstack[index], &vstack[index + 1],
            (size_t) (vtop - index - 1) * sizeof *vstack);
    vtop--;
    vsp--;

    return entry;
}

static int pin_index(int val)
{
    int pin;

    for (pin = 0; pin != npins; pin++)
        if (pin_val[pin] == val)
            return pin;

    return -1;
}

/* The entry on top becomes the pin of `val`. */
static void pin_top(int val)
{
    Value entry;

    if (npins == MAX_PINS) {
        fail = "internal: too many pins";
        return;
    }
    entry = stack_remove(vtop - 1);
    stack_insert(npins, &entry);
    pin_val[npins++] = val;
}

/* A value in its register, pinned there. */
static void pin_add(int val)
{
    Value entry;

    memset(&entry, 0, sizeof entry);
    entry.kind = VAL_REG;
    entry.type = vals[val].type;
    entry.val = vals[val].reg;
    stack_insert(vtop, &entry);
    pin_top(val);
}

static Value pin_take(int pin)
{
    int at;

    for (at = pin; at + 1 < npins; at++)
        pin_val[at] = pin_val[at + 1];
    npins--;

    return stack_remove(pin);
}

/* Each pin back in its register, wherever the classic backend moved it. */
static void pins_restore(void)
{
    int tries, pin;

    for (tries = 0; tries != 4; tries++) {
        int moved = 0;

        for (pin = 0; pin != npins; pin++) {
            Value *entry = &vstack[pin];
            int home = vals[pin_val[pin]].reg;

            if (entry->kind == VAL_REG && entry->val == home)
                continue;
            force_into(entry, home);
            moved = 1;
        }
        if (!moved)
            return;
    }
    fail = "internal: a value would not go back to its register";
}

/* The pins let go of, with every value in its register: at the end of a
 * block, where nothing else is left on the stack. */
static void pins_clear(void)
{
    pins_restore();
    if (vtop != npins)
        fail = "internal: values left on the stack at the end of a block";
    vtop = 0;
    vsp = vstack;
    npins = 0;
}

/* Whether every pin is in its register: after code that jumps, where one
 * moved would be somewhere else on the path that jumped. */
static void pins_check(void)
{
    int pin;

    for (pin = 0; pin != npins; pin++)
        if (vstack[pin].kind != VAL_REG || vstack[pin].val != vals[pin_val[pin]].reg)
            fail = "a jump with a value out of its register";
}

/* At the top of a block: every value that lives in a register and is live
 * there, pinned in it -- each edge into the block left it there. */
static void pins_block_start(int blk)
{
    int val;

    for (val = 0; val != nvals && !fail; val++)
        if (vals[val].reg != HOME_SLOT && LIVE_BIT(live_top, blk, val))
            pin_add(val);
}

/* After an instruction: the pins of the values it read last let go of,
 * where it did not take them as its operands. */
static void pins_release(const Ins *insn)
{
    int operand, pin;

    for (operand = 0; operand != insn->nin; operand++)
        if (insn->kills & (1 << operand) && insn->in[operand].val >= 0) {
            pin = pin_index(insn->in[operand].val);
            if (pin >= 0)
                (void) pin_take(pin);
        }
}

/* An entry's attributes made those it had where the first pass read it:
 * what the relabelling calls between did. */
static void relabel_entry(Value *entry, const Value *attr)
{
    entry->type = attr->type;
    entry->ext = attr->ext;
    entry->quals = attr->quals;
    entry->bits = attr->bits;
}

/* Operand `ent` of the instruction at `at`, made on top of the classic
 * stack from where it lives: made again if a constant, read from its slot,
 * or its register -- the pin itself when this is its last read, a copy of
 * it otherwise. */
static void reg_operand(const Ins *insn, int operand, int at)
{
    const Ent *ent = &insn->in[operand];
    int val = ent->val, pin, last_read = insn->kills & (1 << operand);

    if (val == S_CONST || vals[val].reg == HOME_SLOT) {
        load(ent);
        return;
    }
    pin = pin_index(val);
    if (pin < 0) {
        fail = "internal: a value not in its register";
        return;
    }
    if (last_read) {
        Value entry = pin_take(pin);

        stack_insert(vtop, &entry);
    } else if (vstack[pin].kind == VAL_REG) {
        int to = reg_alloc();

        if (vstack[pin].kind == VAL_REG) {
            push_rr(vstack[pin].val);   /* not ex de, hl: that would move */
            pop_rr(to);                 /* what the pin holds as well */
            vpush(VAL_REG, vals[val].type, to);
        } else {
            vpush(vstack[pin].kind, vals[val].type, vstack[pin].val);
        }
    } else {
        vpush(vstack[pin].kind, vals[val].type, vstack[pin].val);
    }

    /* As load() does for a slot's value. */
    if (ent->attr.type != vals[val].type) {
        if (type_pointer(ent->attr.type) && type_pointer(vals[val].type))
            vset_type(ent->attr.type, ent->attr.ext);
        else
            vconvert(ent->attr.type);
    }
    vset_ext(ent->attr.ext);
    vset_quals(ent->attr.quals);
    if (ent->attr.bits)
        vset_bits(ent->attr.bits);
}

/* An instruction's operands, in order, at the top of the classic stack:
 * the ones left there by the instructions before are there already, and
 * the rest are made and put in between them. */
static void reg_operands(const Ins *insn, int at)
{
    int nfwd = 0, operand, base;

    for (operand = 0; operand != insn->nin; operand++)
        if (insn->in[operand].val >= 0 && vals[insn->in[operand].val].fwd)
            nfwd++;
    base = vtop - nfwd;
    if (base < npins) {
        fail = "internal: an operand left on the stack is not there";
        return;
    }
    for (operand = 0; operand != insn->nin && !fail; operand++) {
        const Ent *ent = &insn->in[operand];
        int pins_before = npins;

        if (ent->val >= 0 && vals[ent->val].fwd) {
            relabel_entry(&vstack[base + operand], &ent->attr);
            continue;
        }
        reg_operand(insn, operand, at);
        base -= pins_before - npins;    /* a pin taken from below */
        if (vtop - 1 != base + operand) {
            Value entry = stack_remove(vtop - 1);

            stack_insert(base + operand, &entry);
        }
    }
}

/* The value just made, on top of the classic stack, to where it lives: its
 * register, whatever is there moved out, and pinned there; its slot; or
 * left on the stack for the instruction that reads it. */
static void settle_as(int val, int converted);

static void settle(int val)
{
    settle_as(val, 0);
}

static void settle_as(int val, int converted)
{
    int pin;

    if (vals[val].fwd)
        return;
    if (!vals[val].used) {
        gen_discard();
        return;
    }
    if (vals[val].reg == HOME_SLOT) {
        vstore_local(vals[val].slot, vals[val].type);
        vdrop();
        return;
    }
    pin = pin_index(val);
    if (pin >= 0)
        (void) pin_take(pin);           /* the other arm of a ?: set it */

    /* A narrow value as the classic backend makes it in a register has its
     * type but not yet its width -- `w - 1` of an unsigned char is -1 there
     * until a store to a byte cuts it. Kept in a register, it is cut now. */
    if (!converted && type_size(vals[val].type) < ACC_INT_SIZE
        && (vsp - 1)->kind == VAL_REG) {
        (vsp - 1)->type = TY_INT;
        vconvert(vals[val].type);
    }
    force_into(vsp - 1, vals[val].reg);
    (vsp - 1)->type = vals[val].type;
    pin_top(val);
}

/* The copies an edge makes into the phis of the block it goes to, with
 * values in registers: every value the edge brings loaded and held first,
 * and only then each put where its phi lives -- a register by force_into,
 * which moves whatever is in the way, and a slot by a store. The values
 * that go on into the block as they are stay pinned throughout. The stack
 * is empty before and after. */
static void edge_moves(int from, int to)
{
    int sources[MAX_LOCALS * 4], dests[MAX_LOCALS * 4], nmoves = 0;
    int pred, phi, val, move;

    for (val = 0; val != nvals && !fail; val++)
        if (vals[val].reg != HOME_SLOT && LIVE_BIT(live_in, to, val))
            pin_add(val);

    for (pred = 0; pred != preds[to].count; pred++) {
        if (preds[to].at[pred] != from)
            continue;
        for (phi = 0; phi != nphis && !fail; phi++) {
            const Phi *join = &phis[phi];
            int source, again = 0;

            if (!join->live || join->block != to)
                continue;
            source = join->in[pred];
            if (source < 0 || source == join->val
                || (vals[source].reg == vals[join->val].reg
                    && (vals[source].reg != HOME_SLOT
                        || vals[source].slot == vals[join->val].slot))
                || nmoves == (int) (sizeof dests / sizeof dests[0]))
                continue;
            for (move = 0; move != nmoves; move++)
                if (sources[move] == source)
                    again = 1;
            if (vals[source].reg == HOME_SLOT) {
                vpush_local(vals[source].slot, vals[source].type);
            } else if (!again && pin_index(source) < 0) {
                vpush(VAL_REG, vals[source].type, vals[source].reg);
            } else {
                int held = pin_index(source), copy = reg_alloc();

                if (held < 0) {
                    /* Read by an edge before: in the register the first
                     * read left it in, still on the stack. */
                    for (move = 0; move != nmoves; move++)
                        if (sources[move] == source)
                            held = vtop - nmoves + move;
                }
                if (vstack[held].kind == VAL_REG) {
                    push_rr(vstack[held].val);
                    pop_rr(copy);
                    vpush(VAL_REG, vals[source].type, copy);
                } else {
                    vpush(vstack[held].kind, vals[source].type,
                          vstack[held].val);
                }
            }
            (void) force_reg(vsp - 1);  /* held now, not read later */
            sources[nmoves] = source;
            dests[nmoves++] = join->val;
        }
        break;                          /* one edge's worth: the first */
    }

    /* The last first: each is on top when its turn comes. */
    for (move = nmoves - 1; move >= 0 && !fail; move--) {
        int dest = dests[move];

        if (vals[dest].reg == HOME_SLOT) {
            vstore_local(vals[dest].slot, vals[dest].type);
            vdrop();
        } else {
            force_into(vsp - 1, vals[dest].reg);
            (vsp - 1)->type = vals[dest].type;
            pin_top(dest);
        }
    }
    pins_clear();
}

/* The pins taken off the stack while a branch is made, and put back: the
 * classic backend wants the condition alone there, and asks it to be to
 * jump on a comparison's flags. With nothing else there it reaches for HL
 * only, which no value lives in. */
static Value hidden[MAX_PINS];

static void pins_hide(void)
{
    Value cond = vstack[vtop - 1];

    memcpy(hidden, vstack, (size_t) npins * sizeof *hidden);
    vstack[0] = cond;
    vtop = 1;
    vsp = vstack + 1;
}

static void pins_unhide(void)
{
    if (vtop != 0) {
        fail = "internal: a branch left values on the stack";
        return;
    }
    memcpy(vstack, hidden, (size_t) npins * sizeof *hidden);
    vtop = npins;
    vsp = vstack + npins;
}

static void emit_branch_regs(const Ins *insn, int blk, int at)
{
    int target = insn->target, invert;

    reg_operands(insn, at);
    if (target < 0) {
        vdrop();
        return;
    }

    /* Turned over while the pins can still be seen: the truth turned over
     * is `== 0`, and vnot is ~, not !. */
    if (type_wide(vtype()))
        vtruth(TK_NE);
    invert = has_phis(target) || block_now[target] < 0 ? insn->sense
                                                       : !insn->sense;
    if (invert)
        vtruth(TK_EQ);
    pins_restore();
    if (vtop != npins + 1) {
        fail = "internal: values left under a branch";
        return;
    }
    pins_hide();
    if (has_phis(target)) {
        GROW(trampolines, ntrampolines, trampolines_cap);
        trampolines[ntrampolines].hole = gen_jump_if_false();
        trampolines[ntrampolines].from = blk;
        trampolines[ntrampolines].to = target;
        ntrampolines++;
    } else if (block_now[target] >= 0) {
        gen_jump_if_true_to(block_now[target]);
    } else {
        jump_forward(gen_jump_if_false(), target);
    }
    pins_unhide();
    pins_check();
}

static void emit_insn_regs(const Ins *insn, int blk, int at)
{
    switch (insn->op) {
    case I_FRAME:
        break;
    case GL_vdrop: {
        int operand;

        /* What was dropped, if it was left on the stack for this. */
        for (operand = 0; operand != insn->nin; operand++)
            if (insn->in[operand].val >= 0 && vals[insn->in[operand].val].fwd)
                vdrop();
        break;
    }
    case GL_gen_stmt_end:
        /* Not while a value waits on the stack across it: the scratch it
         * frees may be where the classic backend put that value. */
        if (vtop == npins) {
            pins_restore();
            call(insn);
        }
        break;
    case I_SET:
        reg_operands(insn, at);
        if (insn->target >= 0) {
            vconvert(vals[insn->target].type);
            settle_as(insn->target, 1);
        } else {
            vdrop();
        }
        break;
    case I_CONV:
        reg_operands(insn, at);
        if (val_pending((vsp - 1)->kind)
            && type_size(insn->local_type) < ACC_INT_SIZE)
            force_reg(vsp - 1);
        vconvert(insn->local_type);
        settle_as(insn->res, 1);
        break;
    case I_STEP:
        reg_operands(insn, at);
        vpush_const(1, TY_INT);
        vapply((unsigned char) insn->step_op, type_narrow(insn->local_type));
        vconvert(insn->local_type);
        settle_as(insn->res, 1);
        break;
    case I_JMP:
        pins_clear();
        if (insn->target < 0)
            break;                      /* never landed: nothing jumps */
        edge_moves(blk, insn->target);
        if (block_now[insn->target] >= 0)
            gen_jump_to(block_now[insn->target]);
        else if (insn->target != blk + 1 || block_last[blk] != at)
            jump_forward(gen_jump(), insn->target);
        break;
    case I_BR:
        emit_branch_regs(insn, blk, at);
        break;
    case GL_gen_switch_load:
        pins_restore();
        gen_switch_load((int) insn->rec->arg[0], (Type) insn->rec->arg[1]);
        pins_check();
        break;
    case GL_gen_switch_case:
        pins_restore();
        gen_switch_case(insn->rec->arg[0], (uint32_t) insn->rec->arg[1],
                        (Type) insn->rec->arg[2], block_now[insn->target],
                        (int) insn->rec->arg[4]);
        pins_check();
        break;
    case GL_gen_return:
        /* Nothing is kept past a return, and gen_return counts what is
         * on the stack. */
        reg_operands(insn, at);
        while (npins)
            (void) pin_take(npins - 1);
        call(insn);
        break;
    default:
        reg_operands(insn, at);
        call(insn);
        if (insn->res >= 0)
            settle(insn->res);
        else if (pushes(insn->op))
            vdrop();                    /* a void, or a constant made again */
        break;
    }
    pins_release(insn);
    if (vtop == npins && !fail)
        pins_restore();
}

static void emit_regs(void)
{
    int at, blk, local;

    block_now = malloc((size_t) nblocks * sizeof *block_now);
    if (!block_now)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != nblocks; at++)
        block_now[at] = -1;
    npending = 0;
    nmoved = 0;
    ntrampolines = 0;
    npins = 0;

    call(&insns[0]);
    for (at = 1; at != ninsns; at++)
        if (insns[at].op == I_FRAME && !frame_of_value(&insns[at])) {
            Ins frame_call = insns[at];

            frame_call.op = frame_call.rec->op;
            call(&frame_call);
        }
    give_slots();

    /* Each parameter that is values now, from its slot into its first
     * value's register or slot. */
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param) {
            int val = locals[local].entry_val;

            if (vals[val].reg == HOME_SLOT
                && vals[val].slot == locals[local].offset)
                continue;               /* its own slot: there already */
            vpush_local(locals[local].offset, locals[local].type);
            if (vals[val].reg == HOME_SLOT) {
                vstore_local(vals[val].slot, vals[val].type);
                vdrop();
            } else {
                force_into(vsp - 1, vals[val].reg);
                (vsp - 1)->type = vals[val].type;
                pin_top(val);
            }
        }
    pins_clear();

    for (blk = 0; blk != nblocks && !fail; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;
        int falls = 1;

        if (blk)
            emit_block_start(blk);
        pins_block_start(blk);
        for (at = blocks[blk].first; at != end && !fail; at++) {
            if (at == 0)
                continue;               /* the prologue, made above */
            emit_insn_regs(&insns[at], blk, at);
            if (at == end - 1 && (insns[at].op == I_JMP
                                  || insns[at].op == GL_gen_return))
                falls = 0;
        }
        if (fail)
            break;
        pins_clear();
        if (falls && blk + 1 < nblocks)
            edge_moves(blk, blk + 1);
        if (!falls)
            emit_trampolines();
    }

    if (ntrampolines && !fail) {
        int over = gen_jump();

        emit_trampolines();
        gen_label(over);
    }
    free(block_now);
    block_now = NULL;
}

/* Where a value lives, for the dump: on the stack, in a register, or in
 * its web's slot, and the web. */
static const char *where(int val)
{
    static char text[40];

    if (!regs_on())
        return "";
    if (vals[val].fwd)
        return " (stack)";
    snprintf(text, sizeof text, " (%s w%d)",
             vals[val].reg == R_BC ? "bc" : vals[val].reg == R_DE ? "de" : "slot",
             web_root(val));

    return text;
}

/* OPTACC_SSA_DUMP: the form, an instruction a line, before code is made
 * from it. */
static void dump(void)
{
    int at, block = 0, operand;

    fprintf(stderr, "ssa of %s:\n", name_text(sym_at(gl_fn)->name));
    for (at = 0; at != ninsns; at++) {
        const Ins *insn = &insns[at];
        const char *name = insn->op < GL_COUNT ? gl_names[insn->op]
                           : insn->op == I_BR ? "br" : insn->op == I_JMP ? "jmp"
                           : insn->op == I_SET ? "set" : insn->op == I_CONV ? "conv"
                           : insn->op == I_STEP ? "step" : "frame";

        while (block < nblocks && blocks[block].first <= at) {
            int phi, pred;

            fprintf(stderr, " b%d:\n", block);
            for (phi = 0; phi != nphis; phi++) {
                if (phis[phi].block != block || !phis[phi].live)
                    continue;
                fprintf(stderr, "      phi v%d%s of local %d:", phis[phi].val,
                        where(phis[phi].val), locals[phis[phi].local].offset);
                for (pred = 0; pred != preds[block].count; pred++)
                    fprintf(stderr, " b%d:v%d", preds[block].at[pred],
                            phis[phi].in[pred]);
                fprintf(stderr, "\n");
            }
            block++;
        }
        fprintf(stderr, "  %3d %-20s", at, name);
        for (operand = 0; operand != insn->nin; operand++) {
            const Ent *ent = &insn->in[operand];

            if (ent->val >= 0)
                fprintf(stderr, " v%d", ent->val);
            else if (ent->val == S_CONST)
                fprintf(stderr, " #%d", ent->attr.val);
            else
                fprintf(stderr, " void");
        }
        if (insn->res >= 0)
            fprintf(stderr, " -> v%d%s", insn->res, where(insn->res));
        if (insn->op == I_BR)
            fprintf(stderr, " when %d to b%d", insn->sense, insn->target);
        else if (insn->op == I_JMP)
            fprintf(stderr, " to b%d", insn->target);
        else if (insn->op == I_SET)
            fprintf(stderr, " into v%d", insn->target);
        fprintf(stderr, "\n");
    }
}

/* What one function's form held, given back before the next. */
static void forget(void)
{
    int at;

    for (at = 0; at != nphis; at++)
        free(phis[at].in);
    nphis = 0;
    for (at = 0; succs && at != nblocks; at++) {
        free(succs[at].at);
        free(preds[at].at);
        free(dom_kids[at].at);
    }
    free(succs);
    free(preds);
    free(dom_kids);
    free(block_last);
    free(rpo);
    free(rpo_num);
    free(idom);
    free(repl);
    free(live_in);
    free(live_out);
    free(live_top);
    free(clash_bits);
    live_in = live_out = live_top = clash_bits = NULL;
    succs = preds = dom_kids = NULL;
    block_last = rpo = rpo_num = idom = repl = NULL;
}

int ssa_generate(const char **why)
{
    int *keep = malloc((size_t) gl_n * sizeof *keep + 1), rec_at;

    if (!keep)
        acc_error("out of memory for the SSA form");
    ninsns = nvals = nblocks = nholes = nheres = nstk = 0;
    fail = NULL;
    keep_records(gl_log, gl_n, keep);

    new_block(-1);
    if (gl_n == 0 || gl_log[0].op != GL_gen_func_begin)
        fail = "a log that does not begin with the function";
    else
        new_insn(GL_gen_func_begin, &gl_log[0]);
    for (rec_at = 1; rec_at < gl_n && !fail; rec_at++)
        if (keep[rec_at])
            build_one(&gl_log[rec_at]);
    free(keep);
    if (!fail && nstk)
        fail = "values left on the stack at the end";
    for (rec_at = 0; rec_at != nholes && !fail; rec_at++)
        if (holes[rec_at].hole > 0 && holes[rec_at].insn >= 0)
            fail = "a jump that never landed";
    if (fail) {
        *why = fail;
        return 0;
    }
    thread_answers();
    to_values();
    if (!fail) {
        live_ranges();
        if (regs_on()) {
            find_forwarded();
            find_clashes();
        }
        coalesce();
        if (regs_on())
            plan_homes();
        if (getenv("OPTACC_SSA_DUMP"))
            dump();
    }
    if (fail || !plan_slots()) {
        *why = fail;
        forget();
        return 0;
    }
    if (regs_on())
        emit_regs();
    else
        emit();
    forget();
    if (fail) {
        *why = fail;
        return -1;                      /* part emitted: the caller stops */
    }

    return 1;
}

#endif
