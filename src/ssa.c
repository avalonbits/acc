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
    I_FRAME                     /* a frame call, laid down with the prologue */
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
} Ins;

typedef struct {
    Type type;                  /* as it is stored: the type it was made at */
    int  slot;                  /* its frame slot */
    int  def, last;             /* the instructions that make and last use it */
} SVal;

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

/* Whether an instruction is a jump, and so has a block for a target. */
static int jumps(const Ins *insn)
{
    return insn->op == I_JMP || insn->op == I_BR
           || insn->op == GL_gen_switch_case;
}

/* Every value's last use, and its live range made to cover every loop it
 * is live across: a jump back from below its last use to between its
 * definition and that use carries it round again. */
static void live_ranges(void)
{
    int at, operand, val, changed;

    for (at = 0; at != ninsns; at++) {
        const Ins *insn = &insns[at];

        for (operand = 0; operand != insn->nin; operand++) {
            val = insn->in[operand].val;
            if (val >= 0 && vals[val].last < at)
                vals[val].last = at;
        }
        if (insn->op == I_SET && insn->target >= 0
            && vals[insn->target].last < at)
            vals[insn->target].last = at;
    }
    do {
        changed = 0;
        for (at = 0; at != ninsns; at++) {
            const Ins *insn = &insns[at];
            int to;

            if (!jumps(insn) || insn->target < 0)
                continue;
            to = blocks[insn->target].first;
            if (to > at)
                continue;
            for (val = 0; val != nvals; val++)
                if (vals[val].def < to && vals[val].last >= to
                    && vals[val].last < at) {
                    vals[val].last = at;
                    changed = 1;
                }
        }
    } while (changed);
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
        vpush_local(vals[ent->val].slot, vals[ent->val].type);
        if (attr->type != vals[ent->val].type)
            vset_type(attr->type, attr->ext);
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
    if (vals[val].last == vals[val].def) {
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

static int plan_slots(void)
{
    int *free_at, val, slot, bytes = 0;

    slot_of = realloc(slot_of, ((size_t) nvals + 1) * sizeof *slot_of);
    slot_size = realloc(slot_size, ((size_t) nvals + 1) * sizeof *slot_size);
    free_at = calloc((size_t) nvals + 1, sizeof *free_at);
    if (!slot_of || !slot_size || !free_at)
        acc_error("out of memory for the SSA form");
    nslots = 0;
    for (val = 0; val != nvals; val++) {
        int size = type_bytes(vals[val].type, 0);

        if (size < ACC_INT_SIZE)
            size = ACC_INT_SIZE;
        for (slot = 0; slot != nslots; slot++)
            if (free_at[slot] < vals[val].def && slot_size[slot] == size)
                break;
        if (slot == nslots) {
            slot_size[slot] = size;
            bytes += size;
            nslots++;
        }
        slot_of[val] = slot;
        free_at[slot] = vals[val].last;
    }
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
        vals[val].slot = slot_at[slot_of[val]];
    free(slot_at);
}

static void emit_branch(const Ins *insn)
{
    load(&insn->in[0]);
    if (insn->target < 0) {
        vdrop();
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

static void emit(void)
{
    int at, block = 0, operand;

    block_now = malloc((size_t) nblocks * sizeof *block_now);
    if (!block_now)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != nblocks; at++)
        block_now[at] = -1;
    npending = 0;
    nmoved = 0;

    /* The prologue, and the frame as the first pass laid it out. */
    call(&insns[0]);
    for (at = 1; at != ninsns; at++)
        if (insns[at].op == I_FRAME) {
            Ins frame_call = insns[at];

            frame_call.op = frame_call.rec->op;
            call(&frame_call);
        }
    give_slots();

    for (at = 1; at != ninsns && !fail; at++) {
        const Ins *insn = &insns[at];

        while (block + 1 < nblocks && blocks[block + 1].first <= at) {
            block++;
            emit_block_start(block);
        }
        switch (insn->op) {
        case I_FRAME:
        case GL_vdrop:
            break;
        case I_SET:
            load(&insn->in[0]);
            if (insn->target >= 0) {
                vconvert(vals[insn->target].type);
                vstore_local(vals[insn->target].slot,
                             vals[insn->target].type);
            }
            vdrop();
            break;
        case I_JMP:
            if (insn->target < 0)
                break;                  /* never landed: nothing jumps */
            if (block_now[insn->target] >= 0)
                gen_jump_to(block_now[insn->target]);
            else if (insn->target != block + 1
                     || blocks[block + 1].first != at + 1)
                jump_forward(gen_jump(), insn->target);
            break;
        case I_BR:
            emit_branch(insn);
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
                vdrop();                /* a void, or a constant made again */
            break;
        }
    }
    while (block + 1 < nblocks && !fail) {
        block++;
        emit_block_start(block);
    }
    free(block_now);
    block_now = NULL;
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
                           : insn->op == I_SET ? "set" : "frame";

        while (block < nblocks && blocks[block].first <= at)
            fprintf(stderr, " b%d:\n", block++);
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
            fprintf(stderr, " -> v%d", insn->res);
        if (insn->op == I_BR)
            fprintf(stderr, " when %d to b%d", insn->sense, insn->target);
        else if (insn->op == I_JMP)
            fprintf(stderr, " to b%d", insn->target);
        else if (insn->op == I_SET)
            fprintf(stderr, " into v%d", insn->target);
        fprintf(stderr, "\n");
    }
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
    live_ranges();
    if (getenv("OPTACC_SSA_DUMP"))
        dump();
    if (!plan_slots()) {
        *why = fail;
        return 0;
    }
    emit();
    if (fail) {
        *why = fail;
        return -1;                      /* part emitted: the caller stops */
    }

    return 1;
}

#endif
