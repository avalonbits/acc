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

#include <limits.h>
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

#include "ssa_int.h"

/* ------------------------------------------------------------------ */
/* the form                                                            */



/* A block's statics: the records of their bytes, and the symbols settled
 * after them -- and the symbols moved with them, to be put back where a
 * function made here is gone back from (ssa_restore). */
static const GenRec **raws;
int nraws;
static int raws_cap;
static int *settles, nsettles, settles_cap;
static struct { int sym, val; } *moved_syms;
static int nmoved_syms, moved_syms_cap;

Ins   *insns;
int    ninsns;
static int insns_cap;
SVal  *vals;
int    nvals;
static int vals_cap;
Block *blocks;
int    nblocks;
static int blocks_cap;

const char *fail;        /* why the function is left to the classic */

static int native_on(void);
static int regs_on(void);
static int *clash_off, *clash_adj;      /* the clashes, a sorted list a value */

/* How deep in loops each block is, and how many calls, weighted so, each
 * value lives across: see find_loops and find_clashes. */
int  *loop_depth;
static long *across_calls;

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

/* An int to int map, emptied for each function by a new epoch rather than
 * by clearing it, so that a big function does not cost every later one. */
typedef struct {
    int *key, *val;
    unsigned *epoch;
    unsigned now;
    int n, mask;
} IntMap;

static void imap_start(IntMap *map)
{
    map->now++;
    map->n = 0;
    if (!map->key) {
        map->mask = 255;
        map->key = malloc(256 * sizeof *map->key);
        map->val = malloc(256 * sizeof *map->val);
        map->epoch = calloc(256, sizeof *map->epoch);
        if (!map->key || !map->val || !map->epoch)
            acc_error("out of memory for the SSA form");
    }
}

static unsigned imap_hash(int key)
{
    return (unsigned) key * 2654435761u;
}

static int *imap_slot(IntMap *map, int key, int add)
{
    unsigned at = imap_hash(key) & (unsigned) map->mask;

    while (map->epoch[at] == map->now) {
        if (map->key[at] == key)
            return &map->val[at];
        at = (at + 1) & (unsigned) map->mask;
    }
    if (!add)
        return NULL;
    if (2 * (map->n + 1) > map->mask) {
        IntMap bigger = *map;
        int old;

        bigger.mask = 2 * map->mask + 1;
        bigger.n = 0;
        bigger.key = malloc(((size_t) bigger.mask + 1) * sizeof *bigger.key);
        bigger.val = malloc(((size_t) bigger.mask + 1) * sizeof *bigger.val);
        bigger.epoch = calloc((size_t) bigger.mask + 1, sizeof *bigger.epoch);
        if (!bigger.key || !bigger.val || !bigger.epoch)
            acc_error("out of memory for the SSA form");
        for (old = 0; old <= map->mask; old++)
            if (map->epoch[old] == map->now)
                *imap_slot(&bigger, map->key[old], 1) = map->val[old];
        free(map->key);
        free(map->val);
        free(map->epoch);
        *map = bigger;

        return imap_slot(map, key, 1);
    }
    map->epoch[at] = map->now;
    map->key[at] = key;
    map->n++;

    return &map->val[at];
}

/* The holes by number: the first not landed yet, each linked to the next
 * of its number; a ?:'s parked answers by their stub, the latest first;
 * the block that starts at each position gen_here gave, the latest. */
static IntMap hole_first, hole_last, stub_latest, here_block;
static int *hole_link;          /* by hole entry: the next, or -1 */
static int  hole_link_cap;

static void holes_start(void)
{
    imap_start(&hole_first);
    imap_start(&hole_last);
    imap_start(&stub_latest);
    imap_start(&here_block);
}

/* A new entry in holes, linked into its number's list or stub's stack. */
static void hole_push(int hole, int insn_at)
{
    GROW(holes, nholes, holes_cap);
    holes[nholes].hole = hole;
    holes[nholes].insn = insn_at;
    if (nholes >= hole_link_cap) {
        hole_link_cap = hole_link_cap ? 2 * hole_link_cap : 256;
        while (hole_link_cap <= nholes)
            hole_link_cap *= 2;
        hole_link = realloc(hole_link, (size_t) hole_link_cap * sizeof *hole_link);
        if (!hole_link)
            acc_error("out of memory for the SSA form");
    }
    hole_link[nholes] = -1;
    if (hole < 0) {
        int *top = imap_slot(&stub_latest, insn_at, 0);

        if (top) {
            hole_link[nholes] = *top;
            *top = nholes;
        } else {
            *imap_slot(&stub_latest, insn_at, 1) = nholes;
        }
    } else {
        int *first = imap_slot(&hole_first, hole, 0);

        if (first && *first >= 0) {
            hole_link[*imap_slot(&hole_last, hole, 0)] = nholes;
        } else {
            *imap_slot(&hole_first, hole, 1) = nholes;
        }
        *imap_slot(&hole_last, hole, 1) = nholes;
    }
    nholes++;
}


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
    vals[nvals].mem = 0;
    vals[nvals].of_cached = 0;

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
    hole_push(hole, insn_at);
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
    int *first, at;

    if (!hole)
        return;
    first = imap_slot(&hole_first, hole, 0);
    if (first && *first >= 0) {
        at = *first;
        *first = hole_link[at];
        insns[holes[at].insn].target = block;
        holes[at].insn = -1;
        return;
    }
    fail = "a label for a jump the log did not make";
}

static int block_at(int old_at)
{
    int *block = imap_slot(&here_block, old_at, 0);

    if (block)
        return *block;
    fail = "a jump back to a place gen_here did not give";

    return 0;
}

/* The scratch of each call compiled in place: where the first pass put its
 * parameters. */
typedef struct {
    int from, size;
} Inlined;

static Inlined *inlined;
static int      ninlined, inlined_cap;

/* The inlined calls' scratch, the regions that overlap merged -- the
 * same scratch is taken again by the next call compiled in place -- and
 * where each is now: a parameter that did not become values is read and
 * written there, in a slot of this frame's own, as the first pass's
 * scratch is the code made here's to use. */
int  nmerged;

static void inline_merge(void)
{
    int at, into;

    for (at = 1; at < ninlined; at++) {         /* by where each starts */
        Inlined key = inlined[at];
        int back = at - 1;

        while (back >= 0 && inlined[back].from > key.from) {
            inlined[back + 1] = inlined[back];
            back--;
        }
        inlined[back + 1] = key;
    }
    nmerged = 0;
    for (at = 0; at != ninlined; at++) {
        if (nmerged && inlined[at].from
            <= inlined[nmerged - 1].from + inlined[nmerged - 1].size) {
            int end = inlined[at].from + inlined[at].size;

            into = nmerged - 1;
            if (end > inlined[into].from + inlined[into].size)
                inlined[into].size = end - inlined[into].from;
        } else {
            inlined[nmerged++] = inlined[at];
        }
    }
}

/* The frame laid out again: each of the first pass's locals that is still
 * read or written in memory, and each inlined call's scratch, where it is
 * now -- a local that became values has no room at all -- so that every
 * offset the log names is moved to its place here. */
typedef struct {
    int from, size, to;
} Moved_local;

static Moved_local *local_moves;
static int          nlocal_moves, local_moves_cap;

static void local_move(int from, int size, int to)
{
    GROW(local_moves, nlocal_moves, local_moves_cap);
    local_moves[nlocal_moves].from = from;
    local_moves[nlocal_moves].size = size;
    local_moves[nlocal_moves].to = to;
    nlocal_moves++;
}

/* Where the first pass's frame offset `offset` is now: a parameter's, and
 * anything not moved, where it was. */
int inline_moved(int offset)
{
    int at;

    for (at = 0; at != nlocal_moves; at++)
        if (offset >= local_moves[at].from
            && offset < local_moves[at].from + local_moves[at].size)
            return offset - local_moves[at].from + local_moves[at].to;

    return offset;
}

/* The merged regions, each a slot of this frame. */
static void inline_slots(void)
{
    int region;

    for (region = 0; region != nmerged; region++)
        local_move(inlined[region].from, inlined[region].size,
                   gen_local(inlined[region].size));
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
    case GL_gen_iy_param: case GL_gen_iy_take: case GL_gen_local_scope:
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
    hole_push(-1 - answer, (int) rec->ret);
    new_block(-1);
}

/* ?:, after the third operand: its value into the answer's slot too, and
 * the join where the answer is read. */
static void build_cond_end(const GenRec *rec)
{
    int to_stub = (int) rec->arg[0], answer = -1, at, join, *top;
    Ins *set;

    top = imap_slot(&stub_latest, to_stub, 0);
    if (top && *top >= 0) {
        at = *top;
        *top = hole_link[at];
        answer = -1 - holes[at].hole;
        holes[at].insn = 0;
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
        /* A constant stays the constant: where the first pass had it in a
         * register -- the answer of a call compiled in place, popped into
         * one and pushed as it -- its record says register, and taken
         * whole, a `1` was passed on as register 0. */
        if (stk[nstk - 1].val == S_CONST && !val_const(rec->top.kind)) {
            Value attr = rec->top;

            attr.kind = stk[nstk - 1].attr.kind;
            attr.val = stk[nstk - 1].attr.val;
            stk[nstk - 1].attr = attr;
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
        /* A block's static: its bytes, made again where the function's
         * code starts (emit_raws). With relocations among them, not yet. */
        if (rec->arg[3]) {
            fail = "a static local's bytes, with addresses";
            return;
        }
        GROW(raws, nraws, raws_cap);
        raws[nraws++] = rec;
        return;
    case GL_gen_settle:
        GROW(settles, nsettles, settles_cap);
        settles[nsettles++] = (int) rec->arg[0];
        return;
    case GL_gen_inline_begin:
        /* A call compiled in place: its parameters are locals in the
         * scratch the first pass gave them, and locals become values. Kept
         * to check that they all did (inline_left). */
        GROW(inlined, ninlined, inlined_cap);
        inlined[ninlined].from = (int) rec->ret;
        inlined[ninlined].size = (int) rec->arg[0];
        ninlined++;
        return;
    case GL_gen_inline_end:
        return;
    case GL_vpush_reg:
        /* The answer of a call compiled in place, popped into a register
         * and pushed as it: the value it was. */
        if (rec != gl_log && rec[-1].op == GL_vpop_reg)
            return;
        fail = "a register pushed by the parser";
        return;
    case GL_gen_stack_take: case GL_gen_stack_mark: case GL_gen_stack_back:
        fail = "an array whose length is known when it runs";
        return;
    case GL_gen_data_begin: case GL_gen_data_end: case GL_gen_pending_clear:
    case GL_gen_data_fixup: case GL_gen_bss_symbol: case GL_gen_bss_reserve:
    case GL_gen_bss_reserve_aligned: case GL_gen_bss_move:
    case GL_gen_bss_forget: case GL_gen_bss_fixup:
    case GL_gen_late_fixup: case GL_gen_slot: case GL_gen_link_fixup:
    case GL_out_rewind: case GL_out_seek:
        fail = "a static local";
        return;
    case GL_gen_mark: case GL_gen_rollback:
        fail = "internal: a mark left in the log";
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
        *imap_slot(&here_block, (int) rec->ret, 1) = block;
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
        int operand, values = 0;

        insn->op = GL_vdrop;            /* folded: nothing to emit */
        for (operand = 0; operand != insn->nin; operand++)
            values += insn->in[operand].val != S_CONST;
        if (!values)
            insn->nin = 0;              /* and nothing to drop: `-2` */
        push(result(rec, -1));
        return;
    }
    /* A struct as a value is the address of it: `p->x` makes one of `*p`
     * on the way to its member. With OPTACC_REGS it may be, if it stays on
     * the classic stack for its one read (checked after find_forwarded);
     * the plain code gives every value a slot, which a struct is not. */
    if (type_is_struct(rec->top.type) && !regs_on()) {
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
    int changed = 1, at, operand, rounds = 0;

    if (!uses)
        acc_error("out of memory for the SSA form");
    while (changed && rounds++ != 64) {    /* two empty blocks may jump to
                                             * each other for ever */
        changed = 0;
        memset(uses, 0, ((size_t) nvals + 1) * sizeof *uses);
        for (at = 0; at != ninsns; at++)
            for (operand = 0; operand != insns[at].nin; operand++)
                if (insns[at].in[operand].val >= 0)
                    uses[insns[at].in[operand].val]++;

        /* A jump to a block that only jumps on goes where that one goes. */
        for (at = 0; at != ninsns; at++) {
            Ins *jump = &insns[at];
            int first, to;

            if ((jump->op != I_JMP && jump->op != I_BR) || jump->target < 0)
                continue;
            for (first = blocks[jump->target].first;
                 first < ninsns && insns[first].block == jump->target
                 && ignorable(&insns[first]); first++)
                ;
            if (first == ninsns || insns[first].block != jump->target
                || insns[first].op != I_JMP)
                continue;
            to = insns[first].target;
            if (to < 0 || to == jump->target || to == insns[at].block)
                continue;
            jump->target = to;
            changed = 1;
        }

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


Local locals[MAX_LOCALS];
int   nlocals;

/* A long or a float the paths join with is more than the copies into a
 * phi can carry: the form is made again with every local that wide left
 * in memory, wide_in_memory, so that the rest of the function -- the int
 * loops around a long checksum -- still become values. */
static const char wide_phi[] = "a long or a float where paths join";
static int wide_in_memory;

/* Slots the first pass reads or holds itself -- a switch's value, the
 * local in IY -- which no local there can be cached for. */
static int pinned[MAX_LOCALS], npinned;
int cached_any;          /* a read was answered from the cache */
static int ncached;             /* how many locals are cached */


Phi *phis;
int  nphis;
static int phis_cap;


IntList *succs, *preds;
static IntList *dom_kids;
int     *rpo_num, *block_last;
static int *idom;

static int *repl;               /* a value read in place of another, or -1 */

static void list_add(IntList *list, int item)
{
    GROW(list->at, list->count, list->cap);
    list->at[list->count++] = item;
}

/* The local at `offset` read and written as `type`. Two locals of sibling
 * scopes may share a slot, and a named local is only ever reached as its
 * own type, so one of another type there is another local. */
static int local_of(int offset, Type type)
{
    int at;

    for (at = 0; at != nlocals; at++)
        if (locals[at].offset == offset && locals[at].type == type)
            return at;

    return -1;
}

/* Whether another local shares `local`'s slot: then a read that nothing
 * of its own type reaches is taken to be of the other's value -- a VLA's
 * length is written as an int and read as unsigned -- and the function is
 * left to the first pass. Only a type of the same kind, signed or not: a
 * slot an inlined call's body gave back is another's of any type, and
 * neither ever reads the other's. */
static int local_shared(int local)
{
    int at;

    for (at = 0; at != nlocals; at++)
        if (at != local && locals[at].offset == locals[local].offset
            && (locals[at].type & ~TY_UNSIGNED)
               == (locals[local].type & ~TY_UNSIGNED))
            return 1;

    return 0;
}

/* Whether every local at `offset` is values now -- and there is one: what
 * the frame keeps, and IY, are by offset alone. */
static int offset_all_values(int offset)
{
    int at, any = 0;

    for (at = 0; at != nlocals; at++)
        if (locals[at].offset == offset) {
            if (!locals[at].ok)
                return 0;
            any = 1;
        }

    return any;
}

/* A local seen with `type`: added, unless its slot's address was taken. */
static void local_seen(int offset, Type type, int ext)
{
    int at;

    if (local_of(offset, type) >= 0 || nlocals == MAX_LOCALS)
        return;
    at = nlocals++;
    locals[at].offset = offset;
    locals[at].type = type;
    locals[at].ext = ext;
    locals[at].ok = !type_is_struct(type) && local_of(offset, TY_VOID) < 0
                    && !(wide_in_memory && type_wide(type));
    locals[at].is_param = offset > 0;
    locals[at].entry_val = -1;
    locals[at].cached = 0;
}

/* Every local at `offset` ruled out, and any seen there later: its address
 * is taken, or the first pass reads its slot itself. */
static void local_ruled_out(int offset)
{
    int at;

    for (at = 0; at != nlocals; at++)
        if (locals[at].offset == offset)
            locals[at].ok = 0;
    if (local_of(offset, TY_VOID) < 0 && nlocals < MAX_LOCALS) {
        at = nlocals++;
        locals[at].offset = offset;
        locals[at].type = TY_VOID;
        locals[at].ok = 0;
        locals[at].is_param = offset > 0;
        locals[at].entry_val = -1;
        locals[at].cached = 0;
    }
}

static void local_pinned(int offset)
{
    if (npinned < MAX_LOCALS)
        pinned[npinned++] = offset;
    else
        nlocals = MAX_LOCALS + 1;       /* too many to follow: none, then */
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

    nlocals = npinned = 0;
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
            local_pinned(insn->op == GL_gen_switch_load
                         ? (int) rec->arg[0] : (int) rec->arg[4]);
            break;
        case GL_gen_iy_claim: case GL_gen_iy_param: case GL_gen_iy_take:
            local_pinned((int) rec->arg[0]);
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

/* ------------------------------------------------------------------ */
/* locals cached                                                       */

/* A local whose address is taken stays memory: everything that reads it
 * through a pointer, or in a call, has to find it there. But between
 * those, where nothing may write it, a read gives what was last read or
 * written there -- so with the leaf backend such a local is cached: every
 * write still goes to memory, and a read is answered from the value last
 * read or written, where one is sure to be it on every path. Where it is
 * not, the read is made from memory, and that is the value from then on.
 * Only ints and pointers, three bytes: zap's `p`, whose address the
 * expression parser takes. */

#define CACHE_NONE (-4)         /* memory may have changed: read it */

extern int ssa_cache_off;

static int leaf_on(void);
static int regs_on(void);

/* Whether an instruction writes no memory: anything not known not to is
 * taken to. A local's own writes are cache_barrier's. */
static int writes_no_memory(int op)
{
    switch (op) {
    case GL_vpush_const: case GL_vpush_bss: case GL_vconst_addr:
    case GL_vconst_bss: case GL_vpush_const_long: case GL_vpush_const_wide:
    case GL_vpush_const_float: case GL_vconvert: case GL_vpush_local:
    case GL_vapply: case GL_vaddr_local: case GL_vaddr_array: case GL_vderef:
    case GL_vmember: case GL_vneg: case GL_vnot: case GL_vdrop:
    case GL_gen_discard: case GL_gen_stmt_end: case GL_gen_value_end:
    case GL_vpush_function: case GL_vpush_global_addr: case GL_vcast:
    case GL_gen_here: case GL_gen_jump: case GL_gen_jump_to:
    case GL_gen_jump_if_false: case GL_gen_jump_if_true_to:
    case GL_gen_switch_load: case GL_gen_switch_case: case GL_gen_logic_left:
    case GL_gen_logic_right: case GL_vtruth: case GL_vdup: case GL_vswap:
    case GL_gen_cond_begin: case GL_gen_cond_middle: case GL_gen_cond_end:
    case GL_gen_cond_middle_void: case GL_gen_cond_end_void: case GL_gen_label:
    case GL_gen_return: case GL_gen_data: case GL_gen_func_begin:
    case I_BR: case I_JMP: case I_FRAME:
        return 1;
    }

    return 0;
}

/* Whether `insn` may change cached `local`'s memory other than by a write
 * to it by name: anything that writes memory, or a write to another local
 * kept in memory whose bytes overlap it -- two of sibling scopes may share
 * a slot. */
static int cache_barrier(const Ins *insn, int local)
{
    const Local *cached = &locals[local];
    int op = insn->op;

    if (op == GL_vstore_local || op == GL_vprefix_local || op == GL_vpostfix_local) {
        int offset = (int) insn->rec->arg[0], other;
        Type type = (Type) insn->rec->arg[1];
        int bytes = type_wide(type) ? type_wide_bytes(type) : type_size(type);

        if (offset == cached->offset && type == cached->type)
            return 0;
        other = local_of(offset, type);
        if (other >= 0 && locals[other].ok)
            return 0;                   /* values: no memory */

        return offset < cached->offset + ACC_INT_SIZE
               && cached->offset < offset + bytes;
    }

    return !writes_no_memory(op);
}

/* Whether `insn` reads or writes cached `local` by name, and if so in *known
 * whether the local's value is known after it. */
static int cache_event(const Ins *insn, int local, int *known)
{
    int op = insn->op, res = insn->res;

    if ((op != GL_vpush_local && op != GL_vstore_local && op != GL_vprefix_local
         && op != GL_vpostfix_local)
        || (int) insn->rec->arg[0] != locals[local].offset
        || (Type) insn->rec->arg[1] != locals[local].type)
        return 0;
    *known = op != GL_vpostfix_local && res >= 0
             && vals[res].type == locals[local].type;

    return 1;
}

/* Whether a loop reads or writes `local` by name. */
static int in_a_loop(int local)
{
    int at, known;

    for (at = 0; at != ninsns; at++)
        if (loop_depth[insns[at].block] && cache_event(&insns[at], local, &known))
            return 1;

    return 0;
}

/* Which locals are cached, each with a value standing for its memory. */
static void choose_cached(void)
{
    int local, mode = ssa_cache_off;

    /* OPTACC_CACHE_LOOPS: the first way caches only a loop's locals, as
     * OPTACC_PICK=0 keeps the first way -- for test/optacc.sh. */
    if (mode == CACHE_ALL && getenv("OPTACC_CACHE_LOOPS"))
        mode = CACHE_LOOPS;
    cached_any = ncached = 0;
    if (mode == CACHE_LOOPS)
        find_loops();               /* which blocks are in one: in_a_loop */
    for (local = 0; local != nlocals; local++) {
        Local *one = &locals[local];
        int pin;

        one->cached = leaf_on() && regs_on() && mode != CACHE_NONE
                      && !one->ok && one->type != TY_VOID
                      && !type_is_struct(one->type)
                      && type_size(one->type) == ACC_INT_SIZE
                      && !type_float(one->type);
        for (pin = 0; pin != npinned; pin++)
            if (pinned[pin] == one->offset)
                one->cached = 0;
        if (one->cached && mode == CACHE_LOOPS && !in_a_loop(local))
            one->cached = 0;
        if (one->cached) {
            one->mem_val = new_val(one->type, -1);
            vals[one->mem_val].mem = one->offset;
            ncached++;
        }
    }
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

        if (!locals[local].ok && !locals[local].cached)
            continue;
        memset(writes, 0, (size_t) nblocks * sizeof *writes);
        memset(has_phi, 0, (size_t) nblocks * sizeof *has_phi);
        if (locals[local].is_param || locals[local].cached) {
            writes[0] = 1;
            work[nwork++] = 0;
        }
        for (at = 0; at != ninsns; at++) {
            const Ins *insn = &insns[at];
            int known;

            /* A cached local's every read and write is a value of its own,
             * and so is every barrier, after which it is not known. */
            if (locals[local].cached && rpo_num[insn->block] >= 0
                && !writes[insn->block]
                && (cache_event(insn, local, &known)
                    || cache_barrier(insn, local))) {
                writes[insn->block] = 1;
                work[nwork++] = insn->block;
                continue;
            }
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
                if (locals[local].cached)
                    vals[phis[nphis].val].of_cached = local + 1;
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
    int root = val, next;

    while (root >= 0 && repl[root] != -1 && repl[root] != root) {
        if (repl[root] == UNDEF) {
            root = UNDEF;               /* read before any write */
            break;
        }
        root = repl[root];
    }

    /* Each on the way pointed at the end: a chain is walked once. */
    while (val >= 0 && val != root && repl[val] != -1 && repl[val] != val
           && repl[val] != UNDEF) {
        next = repl[val];
        repl[val] = root;
        val = next;
    }

    return root;
}

/* The operand an instruction reads, with what it stood for resolved. */
static void resolve(Ent *ent)
{
    if (ent->val < 0 && ent->val != UNDEF)
        return;
    if (ent->val >= 0)
        ent->val = find_val(ent->val);
    if (ent->val == UNDEF) {
        ent->val = S_CONST;             /* a read before any write: 0 */
        ent->attr.kind = VAL_CONST;
        ent->attr.val = 0;
        ent->wide = 0;
    }
}

/* `++` or `--` of a cached local whose value `old` is known, made a step
 * to a new value -- written to the local's memory as well, by the code
 * made for it -- and the expression's value the old or the new. */
static void cache_step(Ins *insn, int at, int local, int old)
{
    int result = insn->res, stepped = new_val(locals[local].type, at);
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
    insn->step_op = (int) insn->rec->arg[3];
    insn->local_type = locals[local].type;
    insn->nin = 1;
    insn->in[0] = operand_ent;
    insn->res = stepped;
    repl[result] = insn->op == GL_vprefix_local ? stepped : old;
    insn->op = I_STEP;
    insn->mem_store = 1;
    vals[stepped].of_cached = local + 1;
}

/* Each block, in the dominator tree's order: every read of a local turned
 * into the value that reaches it, every write into a new value, and each
 * successor's phis told what this block brings them. */
static int *cur_top;            /* by local: its newest push, or -1 */
static int *undo;               /* the locals pushed, in the order pushed */
static int *undo_val;           /* each push's value */
static int *undo_prev;          /* the push of the same local before it */
static int  nundo, undo_cap;

/* What a block's successors' phis are told, by edge: the successor and
 * the edge's index among its predecessors, listed by the edge's source,
 * and each block's phis, the dead ones too. */
static int *rn_edge_off, *rn_edge_to, *rn_edge_pred;
static int *rn_phi_head, *rn_phi_next;

static void def_push(int local, int val)
{
    if (nundo == undo_cap) {
        undo_cap = undo_cap ? 2 * undo_cap : 256;
        undo = realloc(undo, (size_t) undo_cap * sizeof *undo);
        undo_val = realloc(undo_val, (size_t) undo_cap * sizeof *undo_val);
        undo_prev = realloc(undo_prev, (size_t) undo_cap * sizeof *undo_prev);
        if (!undo || !undo_val || !undo_prev)
            acc_error("out of memory for the SSA form");
    }
    undo[nundo] = local;
    undo_val[nundo] = val;
    undo_prev[nundo] = cur_top[local];
    cur_top[local] = nundo++;
}

static void rename_lists(void)
{
    int blk, pred, phi, n = 0;

    rn_edge_off = calloc((size_t) nblocks + 2, sizeof *rn_edge_off);
    rn_phi_head = malloc(((size_t) nblocks + 1) * sizeof *rn_phi_head);
    rn_phi_next = malloc(((size_t) nphis + 1) * sizeof *rn_phi_next);
    for (blk = 0; blk != nblocks; blk++)
        n += preds[blk].count;
    rn_edge_to = malloc(((size_t) n + 1) * sizeof *rn_edge_to);
    rn_edge_pred = malloc(((size_t) n + 1) * sizeof *rn_edge_pred);
    if (!rn_edge_off || !rn_phi_head || !rn_phi_next || !rn_edge_to
        || !rn_edge_pred)
        acc_error("out of memory for the SSA form");
    for (blk = 0; blk != nblocks; blk++)
        for (pred = 0; pred != preds[blk].count; pred++)
            rn_edge_off[preds[blk].at[pred] + 2]++;
    for (blk = 0; blk != nblocks; blk++)
        rn_edge_off[blk + 2] += rn_edge_off[blk + 1];
    for (blk = 0; blk != nblocks; blk++)
        for (pred = 0; pred != preds[blk].count; pred++) {
            int at = rn_edge_off[preds[blk].at[pred] + 1]++;

            rn_edge_to[at] = blk;
            rn_edge_pred[at] = pred;
        }
    for (blk = 0; blk != nblocks; blk++)
        rn_phi_head[blk] = -1;
    for (phi = nphis - 1; phi >= 0; phi--) {
        rn_phi_next[phi] = rn_phi_head[phis[phi].block];
        rn_phi_head[phis[phi].block] = phi;
    }
}

static void rename_lists_free(void)
{
    free(rn_edge_off);
    free(rn_edge_to);
    free(rn_edge_pred);
    free(rn_phi_head);
    free(rn_phi_next);
    rn_edge_off = rn_edge_to = rn_edge_pred = rn_phi_head = rn_phi_next = NULL;
}

/* The dominator tree can be thousands deep, so a frame holds no array. */
static void rename_block(int blk)
{
    int mark = nundo, at, end, edge, phi, kid;

#define DEF_PUSH(local, val) def_push(local, val)
#define DEF_TOP(local) (cur_top[local] >= 0 ? undo_val[cur_top[local]] : UNDEF)

    for (phi = rn_phi_head[blk]; phi >= 0; phi = rn_phi_next[phi])
        DEF_PUSH(phis[phi].local, phis[phi].val);
    end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;
    for (at = blocks[blk].first; at != end; at++) {
        Ins *insn = &insns[at];
        int local, operand;

        for (operand = 0; operand != insn->nin; operand++)
            resolve(&insn->in[operand]);
        if (ncached)
            for (local = 0; local != nlocals; local++)
                if (locals[local].cached && cache_barrier(insn, local))
                    DEF_PUSH(local, CACHE_NONE);
        if (insn->op != GL_vpush_local && insn->op != GL_vstore_local
            && insn->op != GL_vprefix_local && insn->op != GL_vpostfix_local)
            continue;
        local = local_of((int) insn->rec->arg[0], (Type) insn->rec->arg[1]);
        if (local >= 0 && locals[local].cached) {
            int known;

            /* Read from the cache where it holds the value; otherwise
             * made in memory as before, and what it read or wrote is the
             * value from here. */
            (void) cache_event(insn, local, &known);
            if (insn->res >= 0 && insn->op != GL_vpostfix_local)
                vals[insn->res].of_cached = local + 1;
            if (insn->op == GL_vpush_local && DEF_TOP(local) >= 0) {
                repl[insn->res] = DEF_TOP(local);
                insn->op = GL_vdrop;
                insn->nin = 0;
                insn->res = -1;
                cached_any = 1;
            } else if ((insn->op == GL_vprefix_local
                        || insn->op == GL_vpostfix_local)
                       && DEF_TOP(local) >= 0) {
                /* ++ or -- of a known value: a step, written through. */
                cache_step(insn, at, local, DEF_TOP(local));
                DEF_PUSH(local, insn->res);
                cached_any = 1;
            } else {
                DEF_PUSH(local, known ? insn->res : CACHE_NONE);
            }
            continue;
        }
        if (local < 0 || !locals[local].ok)
            continue;

        if (insn->op != GL_vstore_local && DEF_TOP(local) == UNDEF
            && local_shared(local))
            fail = "a shared slot read as another type";
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
    for (edge = rn_edge_off[blk]; edge != rn_edge_off[blk + 1]; edge++) {
        int to = rn_edge_to[edge], pred = rn_edge_pred[edge];

        for (phi = rn_phi_head[to]; phi >= 0; phi = rn_phi_next[phi]) {
            int local = phis[phi].local, in = DEF_TOP(local);

            phis[phi].in[pred] = in == CACHE_NONE ? locals[local].mem_val : in;
        }
    }
    for (kid = 0; kid != dom_kids[blk].count; kid++)
        rename_block(dom_kids[blk].at[kid]);
    while (nundo != mark) {
        nundo--;
        cur_top[undo[nundo]] = undo_prev[nundo];
    }
#undef DEF_PUSH
#undef DEF_TOP
}

/* Only the phis something reads are kept: a phi is live if an instruction
 * reads it, or a live phi does. */
static void live_phis(void)
{
    int at, operand, phi, nwork = 0;
    char *used = calloc((size_t) nvals + 1, 1);
    int *phi_of = malloc(((size_t) nvals + 1) * sizeof *phi_of);
    int *work = malloc(((size_t) nphis + 1) * sizeof *work);

    if (!used || !phi_of || !work)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != nvals; at++)
        phi_of[at] = -1;
    for (phi = 0; phi != nphis; phi++)
        phi_of[phis[phi].val] = phi;
    for (at = 0; at != ninsns; at++)
        for (operand = 0; operand != insns[at].nin; operand++)
            if (insns[at].in[operand].val >= 0)
                used[insns[at].in[operand].val] = 1;

    /* A phi is live once read: by an instruction, or by a live phi. */
    for (phi = 0; phi != nphis; phi++)
        if (!phis[phi].live && used[phis[phi].val]) {
            phis[phi].live = 1;
            work[nwork++] = phi;
        }
    while (nwork) {
        int pred;

        phi = work[--nwork];
        for (pred = 0; pred != preds[phis[phi].block].count; pred++) {
            int from = find_val(phis[phi].in[pred]);

            phis[phi].in[pred] = from;
            if (from < 0)
                continue;
            used[from] = 1;
            if (phi_of[from] >= 0 && !phis[phi_of[from]].live) {
                phis[phi_of[from]].live = 1;
                work[nwork++] = phi_of[from];
            }
        }
    }
    free(used);
    free(phi_of);
    free(work);
}

static void to_values(void)
{
    int local, at;
    Phi *phi;
    char *case_into;

    nphis = 0;
    find_locals();
    build_cfg();
    dominators();
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param) {
            locals[local].entry_val = new_val(locals[local].type, 0);
        }
    choose_cached();
    place_phis();

    repl = malloc(((size_t) nvals + 1) * sizeof *repl);
    cur_top = malloc(((size_t) nlocals + 1) * sizeof *cur_top);
    if (!repl || !cur_top)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != nvals; at++)
        repl[at] = -1;
    nundo = 0;
    for (local = 0; local != nlocals; local++) {
        cur_top[local] = -1;
        if (locals[local].ok && locals[local].is_param)
            def_push(local, locals[local].entry_val);
        else if (locals[local].cached)
            def_push(local, CACHE_NONE);
    }
    rename_lists();
    rename_block(0);
    rename_lists_free();

    /* What the blocks the entry does not reach read, resolved as well. */
    for (at = 0; at != ninsns; at++) {
        int operand;

        for (operand = 0; operand != insns[at].nin; operand++)
            resolve(&insns[at].in[operand]);
    }
    live_phis();
    for (at = 0; at != nphis; at++)
        if (phis[at].live && !locals[phis[at].local].cached
            && local_shared(phis[at].local)) {
            int pred;

            for (pred = 0; pred != preds[phis[at].block].count; pred++)
                if (phis[at].in[pred] == UNDEF)
                    fail = "a shared slot read as another type";
        }

    /* What the copies into phis cannot do yet: a phi wider than an int,
     * which a register cannot hold across them, and one a switch's case
     * jumps to, which leaves no place for its copies. */
    case_into = calloc((size_t) nblocks + 1, 1);
    if (!case_into)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != ninsns; at++)
        if (insns[at].op == GL_gen_switch_case
            && insns[at].target >= 0 && insns[at].target < nblocks)
            case_into[insns[at].target] = 1;
    for (phi = phis; phi != phis + nphis && !fail; phi++) {
        if (!phi->live)
            continue;
        if (type_wide(vals[phi->val].type) && !(leaf_on() && regs_on()))
            fail = wide_phi;            /* the leaf backend copies them */
        if (!fail && case_into[phi->block])
            fail = "a switch's case where paths join";
    }
    free(case_into);
    free(cur_top);
    cur_top = NULL;
}

/* Whether the local gen_local put at `from`, `size` bytes, is values now
 * and is given no room. */
static int local_gone(int from, int size)
{
    int local;

    if (!offset_all_values(from))
        return 0;
    for (local = 0; local != nlocals; local++)
        if (locals[local].offset == from && !locals[local].is_param
            && type_bytes(locals[local].type, locals[local].ext) == size)
            return 1;

    return 0;
}

/* The bytes of the first pass's locals kept in the frame. */
static int locals_kept(void)
{
    int at, kept = 0;

    for (at = 1; at != ninsns; at++)
        if (insns[at].op == I_FRAME && insns[at].rec->op == GL_gen_local
            && !local_gone((int) insns[at].rec->ret, (int) insns[at].rec->arg[0]))
            kept += (int) insns[at].rec->arg[0];

    return kept;
}

/* A frame call of the first pass's made again, its local moved to where it
 * is now -- or not made, for a local that is values now. */
void frame_again(const Ins *insn)
{
    Ins frame_call = *insn;

    if (insn->rec->op == GL_gen_local) {
        int from = (int) insn->rec->ret, size = (int) insn->rec->arg[0];

        if (!local_gone(from, size))
            local_move(from, size, gen_local(size));
        return;
    }
    frame_call.op = frame_call.rec->op;
    call(&frame_call);
}

/* A step of a local -- p++ -- moved down past the reads of the value it
 * steps, in its block: `*p++` reads the old p after the step is made, so
 * the two were live at once and could not share a home, and the new one
 * was copied back at the end of every trip. After it, they can, and the
 * step is an inc where the value is. For the code made here, as far down
 * as it goes -- to just before a read of what it makes, or the block's end
 * -- rather than just past the old one's last read, which could put it
 * between an argument and its call and make the argument wait on the
 * stack. Not past another
 * step of the same value, and not in between a comparison and the branch
 * on it. */
static int leaf_mode;             /* see emit_leaf */

static int step_may_pass(const Ins *insn, int old)
{
    int op = insn->op;

    if (op == GL_vderef || op == GL_vmember)
        return insn->nin == 1 && insn->in[0].val == old;

    if (op == I_CONV || op == I_STEP)
        return !insn->mem_store;        /* a local's value, not memory */

    return op != GL_vpush_local && writes_no_memory(op);
}

/* leaf_long_ok's index of each value's uses: see low_index. */
static int *luse_head, *luse_next, *luse_at, luse_cap;
static unsigned char *lphi_in;
static int lindex_ok;

static void sink_steps(void)
{
    int at;

    for (at = 0; at != ninsns; at++) {
        Ins step = insns[at];
        int blk = step.block, last = block_last[blk], read_old = at, to, scan;
        int operand;

        if (step.op != I_STEP || step.nin != 1 || step.in[0].val < 0
            || step.res < 0)
            continue;

        /* Past the block's last too, where that falls on to the next --
         * `*o++ = f(x)` ends one with the store -- not a jump, a branch or
         * a return, which it would then come after. */
        if (insns[last].op != I_BR && insns[last].op != I_JMP
            && insns[last].op != GL_gen_return)
            last++;
        for (scan = at + 1; scan < last; scan++) {
            int reads_old = 0, reads_new = 0;

            for (operand = 0; operand != insns[scan].nin; operand++) {
                reads_old |= insns[scan].in[operand].val == step.in[0].val;
                reads_new |= insns[scan].in[operand].val == step.res;
            }
            /* Past a step of something else -- `*o++ = *s++` -- but not
             * of this: the two would change places. */
            if (reads_new || (insns[scan].op == I_STEP && reads_old))
                break;

            /* One written through to a cached local's memory, past
             * nothing that may read or write memory but a read through
             * the old value itself -- `*p++` -- which is in the same
             * expression, and unsequenced with the step anyway. */
            if (step.mem_store && !step_may_pass(&insns[scan], step.in[0].val))
                break;
            if (reads_old)
                read_old = scan;
        }
        if (read_old == at)
            continue;                   /* nothing reads the old one after */

        /* The hybrid path's homes are planned for a step just past its
         * last read, and got worse -- a pointer in DE, a counter spilled
         * -- for one further down; the code made here, better. */
        to = leaf_mode ? scan - 1 : read_old;

        /* Not between a comparison and the branch on it: above it. */
        if (to + 1 == block_last[blk] && insns[block_last[blk]].op == I_BR
            && insns[to].res >= 0)
            to--;
        if (to < read_old)
            continue;
        memmove(&insns[at], &insns[at + 1], (size_t) (to - at) * sizeof *insns);
        insns[to] = step;
        lindex_ok = 0;                  /* leaf_long_ok's uses moved */
        at--;                           /* what moved up into its place, next:
                                         * another step it went past */
    }
}

/* Where each value is live, as the interval its slot is kept for: every
 * position from the first to the last at which it is live -- a value is
 * live into a block that reads it before writing it, and out of one that a
 * successor needs it live into, or whose successor's phi reads it. Layout
 * order is not flow order: a goto into a loop's middle puts a value's
 * reads above where it is made.
 *
 * Found a value at a time, walking back from each read to where it is
 * made (live_ranges): the work is the size of the answer -- each block a
 * value is live in, once -- where a dataflow over every value in every
 * block was quadratic. The answer is a sorted list of values a block. */
typedef struct {
    int *off;                   /* by block: where its list begins; [nblocks]: the end */
    int *val;
} BlockSets;

static BlockSets live_in, live_out, live_top;

static int by_int(const void *left, const void *right)
{
    int l = *(const int *) left, r = *(const int *) right;

    return l < r ? -1 : l > r;
}

/* Pairs of a block and a value, gathered in any order, made a set a
 * block: counted by block, placed, each block's sorted. */
static void bset_make(BlockSets *set, const int *blk, const int *val, int n)
{
    int at;

    free(set->off);
    free(set->val);
    set->off = calloc((size_t) nblocks + 2, sizeof *set->off);
    set->val = malloc((size_t) n * sizeof *set->val + 1);
    if (!set->off || !set->val)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != n; at++)
        set->off[blk[at] + 1]++;
    for (at = 0; at != nblocks; at++)
        set->off[at + 1] += set->off[at];
    {
        int *fill = malloc(((size_t) nblocks + 1) * sizeof *fill);

        if (!fill)
            acc_error("out of memory for the SSA form");
        memcpy(fill, set->off, (size_t) nblocks * sizeof *fill);
        for (at = 0; at != n; at++)
            set->val[fill[blk[at]]++] = val[at];
        free(fill);
    }
    for (at = 0; at != nblocks; at++)
        qsort(set->val + set->off[at], (size_t) (set->off[at + 1] - set->off[at]),
              sizeof *set->val, by_int);
}

static void bset_free(BlockSets *set)
{
    free(set->off);
    free(set->val);
    set->off = set->val = NULL;
}

/* A growing list of block-and-value pairs. */
typedef struct {
    int *blk, *val, n, cap;
} Pairs;

static void pairs_add(Pairs *pairs, int blk, int val)
{
    if (pairs->n == pairs->cap) {
        pairs->cap = pairs->cap ? pairs->cap * 2 : 256;
        pairs->blk = realloc(pairs->blk, (size_t) pairs->cap * sizeof *pairs->blk);
        pairs->val = realloc(pairs->val, (size_t) pairs->cap * sizeof *pairs->val);
        if (!pairs->blk || !pairs->val)
            acc_error("out of memory for the SSA form");
    }
    pairs->blk[pairs->n] = blk;
    pairs->val[pairs->n++] = val;
}

/* The phis of each block, in order: a list a block, made once a function
 * (phis_bucket), where looking through every phi for a block's was
 * quadratic. */
static int *phi_head, *phi_next;

/* An edge's index among its target's predecessors -- the first, where a
 * switch makes an edge twice: a hash of the edges, made with the phis'
 * lists, for what looks up an edge's phi inputs. */
static int *edge_key_to, *edge_key_from, *edge_idx, edge_mask;

static void edges_hash(void)
{
    int blk, pred, n = 1, k;

    for (blk = 0; blk != nblocks; blk++)
        n += preds[blk].count;
    for (k = 16; k < 2 * n; k *= 2)
        ;
    edge_mask = k - 1;
    edge_key_to = realloc(edge_key_to, (size_t) k * sizeof *edge_key_to);
    edge_key_from = realloc(edge_key_from, (size_t) k * sizeof *edge_key_from);
    edge_idx = realloc(edge_idx, (size_t) k * sizeof *edge_idx);
    if (!edge_key_to || !edge_key_from || !edge_idx)
        acc_error("out of memory for the SSA form");
    for (n = 0; n != k; n++)
        edge_key_to[n] = -1;
    for (blk = 0; blk != nblocks; blk++)
        for (pred = 0; pred != preds[blk].count; pred++) {
            int from = preds[blk].at[pred];
            unsigned h = ((unsigned) blk * 2654435761u ^ (unsigned) from * 40503u) & (unsigned) edge_mask;

            while (edge_key_to[h] >= 0 && !(edge_key_to[h] == blk && edge_key_from[h] == from))
                h = (h + 1) & (unsigned) edge_mask;
            if (edge_key_to[h] < 0) {
                edge_key_to[h] = blk;
                edge_key_from[h] = from;
                edge_idx[h] = pred;
            }
        }
}

static int pred_index(int to, int from)
{
    unsigned h = ((unsigned) to * 2654435761u ^ (unsigned) from * 40503u) & (unsigned) edge_mask;

    while (edge_key_to[h] >= 0) {
        if (edge_key_to[h] == to && edge_key_from[h] == from)
            return edge_idx[h];
        h = (h + 1) & (unsigned) edge_mask;
    }

    return -1;
}

static void phis_bucket(void)
{
    int phi, blk;

    edges_hash();

    phi_head = realloc(phi_head, ((size_t) nblocks + 1) * sizeof *phi_head);
    phi_next = realloc(phi_next, ((size_t) nphis + 1) * sizeof *phi_next);
    if (!phi_head || !phi_next)
        acc_error("out of memory for the SSA form");
    for (blk = 0; blk != nblocks; blk++)
        phi_head[blk] = -1;
    for (phi = nphis - 1; phi >= 0; phi--)
        if (phis[phi].live) {
            phi_next[phi] = phi_head[phis[phi].block];
            phi_head[phis[phi].block] = phi;
        }
}

/* A set of values, sparse: each added, taken out, and walked over in time
 * of its own -- the live values as a block is walked back. */
static int *lset_pos, *lset_mem, lset_n;

static void lset_add(int val)
{
    if (lset_pos[val] >= 0)
        return;
    lset_pos[val] = lset_n;
    lset_mem[lset_n++] = val;
}

static void lset_del(int val)
{
    int at = lset_pos[val];

    if (at < 0)
        return;
    lset_mem[at] = lset_mem[--lset_n];
    lset_pos[lset_mem[at]] = at;
    lset_pos[val] = -1;
}

static void lset_clear(void)
{
    while (lset_n)
        lset_pos[lset_mem[--lset_n]] = -1;
}

static void lset_from(const BlockSets *set, int blk)
{
    int k;

    lset_clear();
    for (k = set->off[blk]; k != set->off[blk + 1]; k++)
        lset_add(set->val[k]);
}

static void lset_alloc(void)
{
    int val;

    lset_pos = realloc(lset_pos, ((size_t) nvals + 1) * sizeof *lset_pos);
    lset_mem = realloc(lset_mem, ((size_t) nvals + 1) * sizeof *lset_mem);
    if (!lset_pos || !lset_mem)
        acc_error("out of memory for the SSA form");
    for (val = 0; val != nvals; val++)
        lset_pos[val] = -1;
    lset_n = 0;
}

/* A bound on the work of the passes that could be quadratic on a large
 * function -- the clashes when everything is live at once, a cached
 * local's values paired: past a constant times the function's size, the
 * function is left to the classic backend, which is linear. */
#define SSA_WORK_PER 512
static long ssa_work, ssa_work_max;

static int work(long amount)
{
    ssa_work += amount;
    if (ssa_work > ssa_work_max && !fail)
        fail = "a function too big for the SSA form's backends";

    return !fail;
}

static void widen(int val, int at)
{
    if (vals[val].def > at)
        vals[val].def = at;
    if (vals[val].last < at)
        vals[val].last = at;
}

static void live_ranges(void)
{
    int blk, at, operand, val, phi_at, local, ndefs = 0, nuses = 0, k;
    int *def_head, *def_next, *def_blk, *def_pos;
    int *use_head, *use_next, *use_blk, *use_pos;     /* use_pos -1: a phi's, out */
    int *in_mark, *out_mark, *def_mark, *def_first, *work_list, nwork;
    Pairs ins = { 0 }, outs = { 0 };

    phis_bucket();
    def_head = malloc(((size_t) nvals + 1) * sizeof *def_head);
    use_head = malloc(((size_t) nvals + 1) * sizeof *use_head);
    in_mark = malloc(((size_t) nblocks + 1) * sizeof *in_mark);
    out_mark = malloc(((size_t) nblocks + 1) * sizeof *out_mark);
    def_mark = malloc(((size_t) nblocks + 1) * sizeof *def_mark);
    def_first = malloc(((size_t) nblocks + 1) * sizeof *def_first);
    work_list = malloc(((size_t) nblocks + 1) * sizeof *work_list);
    k = ninsns + nphis + nlocals + 1;
    def_next = malloc((size_t) k * sizeof *def_next);
    def_blk = malloc((size_t) k * sizeof *def_blk);
    def_pos = malloc((size_t) k * sizeof *def_pos);
    k = ninsns * MAX_OPERANDS + 1;
    for (phi_at = 0; phi_at != nphis; phi_at++)
        if (phis[phi_at].live)
            k += preds[phis[phi_at].block].count;
    use_next = malloc((size_t) k * sizeof *use_next);
    use_blk = malloc((size_t) k * sizeof *use_blk);
    use_pos = malloc((size_t) k * sizeof *use_pos);
    if (!def_head || !use_head || !in_mark || !out_mark || !def_mark || !def_first
        || !work_list || !def_next || !def_blk || !def_pos || !use_next
        || !use_blk || !use_pos)
        acc_error("out of memory for the SSA form");
    for (val = 0; val != nvals; val++)
        def_head[val] = use_head[val] = -1;
    for (blk = 0; blk != nblocks; blk++)
        in_mark[blk] = out_mark[blk] = def_mark[blk] = -1;

#define ADD_DEF(v, b, p) do { def_blk[ndefs] = (b); def_pos[ndefs] = (p);     \
        def_next[ndefs] = def_head[v]; def_head[v] = ndefs++; } while (0)
#define ADD_USE(v, b, p) do { use_blk[nuses] = (b); use_pos[nuses] = (p);     \
        use_next[nuses] = use_head[v]; use_head[v] = nuses++; } while (0)

    /* Where each value is made -- a parameter's, a cached local's memory
     * and a phi's at the top of a block, position -1 -- and read. */
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param)
            ADD_DEF(locals[local].entry_val, 0, -1);
        else if (locals[local].cached)
            ADD_DEF(locals[local].mem_val, 0, -1);
    for (phi_at = 0; phi_at != nphis; phi_at++)
        if (phis[phi_at].live)
            ADD_DEF(phis[phi_at].val, phis[phi_at].block, -1);
    for (blk = 0; blk != nblocks; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;

        for (at = blocks[blk].first; at != end; at++) {
            const Ins *insn = &insns[at];

            for (operand = 0; operand != insn->nin; operand++) {
                val = insn->in[operand].val;
                if (val < 0)
                    continue;
                vals[val].used = 1;
                ADD_USE(val, blk, at);
            }
            if (insn->res >= 0)
                ADD_DEF(insn->res, blk, at);
            if (insn->op == I_SET && insn->target >= 0)
                ADD_DEF(insn->target, blk, at);
        }
    }
    for (phi_at = 0; phi_at != nphis; phi_at++) {
        const Phi *phi = &phis[phi_at];
        int pred;

        if (!phi->live)
            continue;
        for (pred = 0; pred != preds[phi->block].count; pred++)
            if (phi->in[pred] >= 0) {
                vals[phi->in[pred]].used = 1;
                ADD_USE(phi->in[pred], preds[phi->block].at[pred], -1);
            }
    }

    /* Each value: live into a block that reads it before it is made
     * there, out of a block that a successor it is live into follows or
     * whose successor's phi reads it, and on back through the
     * predecessors until the blocks that make it. */
    for (val = 0; val != nvals; val++) {
        int e;

        if (use_head[val] < 0)
            continue;
        for (e = def_head[val]; e >= 0; e = def_next[e])
            if (def_mark[def_blk[e]] != val || def_pos[e] < def_first[def_blk[e]]) {
                if (def_mark[def_blk[e]] != val)
                    def_first[def_blk[e]] = def_pos[e];
                def_mark[def_blk[e]] = val;
                if (def_pos[e] < def_first[def_blk[e]])
                    def_first[def_blk[e]] = def_pos[e];
            }
        nwork = 0;
        for (e = use_head[val]; e >= 0; e = use_next[e]) {
            int b = use_blk[e];

            if (use_pos[e] < 0) {               /* a phi's: out of b */
                if (out_mark[b] != val) {
                    out_mark[b] = val;
                    pairs_add(&outs, b, val);
                }
                if (def_mark[b] == val)
                    continue;
            } else if (def_mark[b] == val && def_first[b] < use_pos[e]) {
                continue;                       /* made before it is read */
            }
            if (in_mark[b] != val) {
                in_mark[b] = val;
                pairs_add(&ins, b, val);
                work_list[nwork++] = b;
            }
        }
        while (nwork) {
            int b = work_list[--nwork], pred;

            for (pred = 0; pred != preds[b].count; pred++) {
                int p = preds[b].at[pred];

                if (out_mark[p] != val) {
                    out_mark[p] = val;
                    pairs_add(&outs, p, val);
                }
                if (def_mark[p] != val && in_mark[p] != val) {
                    in_mark[p] = val;
                    pairs_add(&ins, p, val);
                    work_list[nwork++] = p;
                }
            }
        }
    }
#undef ADD_DEF
#undef ADD_USE
    bset_make(&live_in, ins.blk, ins.val, ins.n);
    bset_make(&live_out, outs.blk, outs.val, outs.n);
    free(ins.blk);
    free(ins.val);
    free(outs.blk);
    free(outs.val);

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
        for (k = live_in.off[blk]; k != live_in.off[blk + 1]; k++)
            widen(live_in.val[k], first);
        for (k = live_out.off[blk]; k != live_out.off[blk + 1]; k++)
            widen(live_out.val[k], last);
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

    free(def_head);
    free(use_head);
    free(in_mark);
    free(out_mark);
    free(def_mark);
    free(def_first);
    free(work_list);
    free(def_next);
    free(def_blk);
    free(def_pos);
    free(use_next);
    free(use_blk);
    free(use_pos);
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

/* The moves kept in order of where they were: one is added after the
 * last but a few places from the end, which the shift costs, and found by
 * halving. */
static void moved_add(int old_at, int len, int new_at)
{
    int at;

    GROW(moved, nmoved, moved_cap);
    at = nmoved++;
    while (at && moved[at - 1].old_at > old_at) {
        moved[at] = moved[at - 1];
        at--;
    }
    moved[at].old_at = old_at;
    moved[at].len = len;
    moved[at].new_at = new_at;
}

/* The move that starts last at or before `at`, or -1. */
static int moved_before(int at)
{
    int lo = 0, hi = nmoved;

    while (lo < hi) {               /* the first that starts past `at` */
        int mid = (lo + hi) / 2;

        if (moved[mid].old_at <= at)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo - 1;
}

/* Where what was at `at` is now: a string's terminator, one past its
 * length, moved with it. */
static int moved_at(int at)
{
    int idx = moved_before(at);

    if (idx >= 0 && at <= moved[idx].old_at + moved[idx].len)
        return at - moved[idx].old_at + moved[idx].new_at;

    return at;
}

int ssa_moved_at(int at)
{
    return moved_at(at);
}

void ssa_moved_add(int old_at, int len, int new_at)
{
    moved_add(old_at, len, new_at);
}

/* The block's statics' bytes, jumped over where the code starts, as the
 * first pass had them where they were declared; their addresses moved
 * with them -- moved_at for the constants that are them, and each symbol
 * among them before it is settled. */
static void emit_raws(void)
{
    int over, at, settle;

    if (!nraws)
        return;
    over = gen_jump();
    for (at = 0; at != nraws; at++) {
        const unsigned char *bytes = (const unsigned char *) gl_kept(raws[at]->arg[2]);
        int len = (int) raws[at]->arg[1], byte;

        moved_add((int) raws[at]->arg[0], len, out_here());
        for (byte = 0; byte != len; byte++)
            out_byte(bytes[byte]);
    }
    for (settle = 0; settle != nsettles; settle++) {
        Sym *sym = sym_at(settles[settle]);

        at = moved_before(sym->val);
        if (at >= 0 && sym->val < moved[at].old_at + moved[at].len) {
            GROW(moved_syms, nmoved_syms, moved_syms_cap);
            moved_syms[nmoved_syms].sym = settles[settle];
            moved_syms[nmoved_syms].val = sym->val;
            nmoved_syms++;
            sym->val += moved[at].new_at - moved[at].old_at;
        }
        gen_settle(settles[settle]);
    }
    gen_label(over);
}

/* A new function: the last one's statics stay where they were made. */
void ssa_keep_moves(void)
{
    nmoved_syms = 0;
}

/* Each symbol emit_raws moved back where the first pass had it: the code
 * made here is being gone back from. */
void ssa_restore(void)
{
    while (nmoved_syms) {
        nmoved_syms--;
        sym_at(moved_syms[nmoved_syms].sym)->val = moved_syms[nmoved_syms].val;
    }
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
    if (ent->val == UNDEF) {
        vpush_const(0, attr->type);     /* read before any write: any
                                         * value will do, and 0 is one */
    } else if (ent->val == S_CONST) {
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
void call(const Ins *insn)
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
    case GL_vpush_local:        vpush_local(inline_moved(ARG(0, int)), ARG(1, Type)); break;
    case GL_vstore_local:       vstore_local(inline_moved(ARG(0, int)), ARG(1, Type)); break;
    case GL_vapply:             vapply(ARG(0, unsigned char), ARG(1, Type)); break;
    case GL_vaddr_local:        vaddr_local(inline_moved(ARG(0, int)), ARG(1, Type)); break;
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
        vprefix_local(inline_moved(ARG(0, int)), ARG(1, Type), ARG(2, int), ARG(3, int));
        break;
    case GL_vpostfix_local:
        vpostfix_local(inline_moved(ARG(0, int)), ARG(1, Type), ARG(2, int), ARG(3, int));
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

        moved_add((int) rec->ret, ARG(1, int), at);
        break;
    }
    case GL_gen_return:
        gen_return(ARG(0, int), (const char *) (intptr_t) arg[1]);
        break;
    case GL_gen_local:          (void) gen_local(ARG(0, int)); break;
    case GL_gen_local_far:      (void) gen_local_far(ARG(0, int)); break;
    case GL_gen_local_scope:    gen_local_scope(ARG(0, int)); break;
    case GL_gen_local_array:    (void) gen_local_array(); break;
    case GL_gen_local_array_size:
        gen_local_array_size(ARG(0, int), ARG(1, int));
        break;
    case GL_gen_iy_claim:
        gen_iy_claim(inline_moved(ARG(0, int)), ARG(1, Type), ARG(2, int));
        break;
    case GL_gen_iy_param:       gen_iy_param(ARG(0, int)); break;
    case GL_gen_iy_take:        gen_iy_take(inline_moved(ARG(0, int))); break;
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
static int     *pending_next;           /* by entry: the block's next */
static int     *pending_head, *pending_tail;  /* by block */
static int      pending_next_cap, pending_blocks_cap;

static int read_counts_made, bc_busy_made;

/* What each emission starts afresh: the pending jumps, and what the leaf
 * backend works out once an emission when it first asks. */
static void emit_start(void)
{
    int blk;

    read_counts_made = bc_busy_made = 0;
    npending = 0;
    if (nblocks > pending_blocks_cap) {
        pending_blocks_cap = nblocks;
        pending_head = realloc(pending_head, (size_t) nblocks * sizeof *pending_head);
        pending_tail = realloc(pending_tail, (size_t) nblocks * sizeof *pending_tail);
        if (!pending_head || !pending_tail)
            acc_error("out of memory for the SSA form");
    }
    for (blk = 0; blk != nblocks; blk++)
        pending_head[blk] = -1;
}

static void jump_forward(int hole, int block)
{
    if (!hole)
        return;
    GROW(pending, npending, pending_cap);
    if (npending >= pending_next_cap) {
        pending_next_cap = pending_next_cap ? 2 * pending_next_cap : 256;
        while (pending_next_cap <= npending)
            pending_next_cap *= 2;
        pending_next = realloc(pending_next, (size_t) pending_next_cap * sizeof *pending_next);
        if (!pending_next)
            acc_error("out of memory for the SSA form");
    }
    pending[npending].hole = hole;
    pending[npending].block = block;
    pending_next[npending] = -1;
    if (pending_head[block] < 0)
        pending_head[block] = npending;
    else
        pending_next[pending_tail[block]] = npending;
    pending_tail[block] = npending;
    npending++;
}

static void emit_block_start(int block)
{
    int at;

    for (at = pending_head[block]; at >= 0; at = pending_next[at])
        if (pending[at].hole) {
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

/* Slots of one size, in the order they were made, for plan_slots to find
 * the first that will do without looking at every slot: with the clashes
 * (OPTACC_REGS), the first no neighbour of the web holds, those it holds
 * marked; without, the first free by the web's start, through a tree of
 * the earliest each run of them is free at. */
typedef struct {
    int size;
    int *slot, n, cap;
    int *tree, leaves;                  /* min free_at, a leaf a slot */
} SizeClass;

static SizeClass *size_class;
static int nsize_classes, size_classes_cap;
static int *plan_free_at;               /* plan_slots' free_at, by slot */

static int free_at_of_class(const SizeClass *c, int pos)
{
    return plan_free_at[c->slot[pos]];
}

static SizeClass *class_of(int size)
{
    int at;

    for (at = 0; at != nsize_classes; at++)
        if (size_class[at].size == size)
            return &size_class[at];
    GROW(size_class, nsize_classes, size_classes_cap);
    memset(&size_class[nsize_classes], 0, sizeof size_class[nsize_classes]);
    size_class[nsize_classes].size = size;

    return &size_class[nsize_classes++];
}

static void class_tree_set(SizeClass *c, int pos, int free_at)
{
    int at = c->leaves + pos;

    c->tree[at] = free_at;
    for (at /= 2; at >= 1; at /= 2)
        c->tree[at] = c->tree[2 * at] < c->tree[2 * at + 1]
                      ? c->tree[2 * at] : c->tree[2 * at + 1];
}

static void class_add(SizeClass *c, int slot, int free_at)
{
    if (c->n == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 16;
        c->slot = realloc(c->slot, (size_t) c->cap * sizeof *c->slot);
        if (!c->slot)
            acc_error("out of memory for the SSA form");
    }
    if (c->n == c->leaves) {            /* the tree grown: made again */
        int at, old = c->leaves;

        c->leaves = c->leaves ? c->leaves * 2 : 16;
        c->tree = realloc(c->tree, (size_t) c->leaves * 2 * sizeof *c->tree);
        if (!c->tree)
            acc_error("out of memory for the SSA form");
        for (at = 0; at != c->leaves; at++)
            c->tree[c->leaves + at] = INT_MAX;
        for (at = 0; at != old; at++)
            c->tree[c->leaves + at] = free_at_of_class(c, at);
        for (at = c->leaves - 1; at >= 1; at--)
            c->tree[at] = c->tree[2 * at] < c->tree[2 * at + 1]
                          ? c->tree[2 * at] : c->tree[2 * at + 1];
    }
    c->slot[c->n] = slot;
    class_tree_set(c, c->n++, free_at);
}

/* The leftmost of a class's slots free before `first`, or -1. */
static int class_free_before(const SizeClass *c, int first)
{
    int at = 1;

    if (!c->n || c->tree[1] >= first)
        return -1;
    while (at < c->leaves)
        at = c->tree[2 * at] < first ? 2 * at : 2 * at + 1;

    return at - c->leaves;
}

static int plan_slots(void)
{
    int *free_at, *taken, val, slot, bytes = 0, at, chosen;

    slot_of = realloc(slot_of, ((size_t) nvals + 1) * sizeof *slot_of);
    slot_size = realloc(slot_size, ((size_t) nvals + 1) * sizeof *slot_size);
    free_at = calloc((size_t) nvals + 1, sizeof *free_at);
    taken = malloc(((size_t) nvals + 1) * sizeof *taken);
    if (!slot_of || !slot_size || !free_at || !taken)
        acc_error("out of memory for the SSA form");
    plan_free_at = free_at;
    nslots = 0;
    for (at = 0; at != nsize_classes; at++)
        size_class[at].n = size_class[at].leaves = 0;   /* the last function's */
    for (val = 0; val != nvals; val++) {
        slot_of[val] = -1;              /* none yet: a clash reads ahead */
        taken[val] = -1;
    }

    /* A slot a web: each of its values in it, for the web's whole span. */
    for (val = 0; val != nvals; val++) {
        int size = type_bytes(vals[val].type, 0), first, last, member;
        SizeClass *c;

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
        c = class_of(size);
        slot = nslots;
        chosen = -1;
        if (clash_off) {
            /* The slots of the webs this one's values clash with, marked;
             * the first of its size not marked. */
            int k, pos;

            for (member = val; member >= 0; member = web_next[member])
                for (k = clash_off[member]; k != clash_off[member + 1]; k++) {
                    int other = web_root(clash_adj[k]);

                    if (slot_of[other] >= 0)
                        taken[slot_of[other]] = val;
                }
            for (pos = 0; pos != c->n; pos++)
                if (taken[c->slot[pos]] != val)
                    break;
            if (pos != c->n) {
                slot = c->slot[pos];
                chosen = pos;
            }
        } else {
            int pos = class_free_before(c, first);

            if (pos >= 0) {
                slot = c->slot[pos];
                chosen = pos;
            }
        }
        if (slot == nslots) {
            slot_size[slot] = size;
            bytes += size;
            nslots++;
            free_at[slot] = last;
            class_add(c, slot, last);
        } else {
            free_at[slot] = last;
            class_tree_set(c, chosen, last);
        }
        slot_of[val] = slot;
    }
    for (val = 0; val != nvals; val++)
        if (!vals[val].fwd && vals[val].reg == HOME_SLOT
            && !(fixed_slot && fixed_slot[web_root(val)]))
            slot_of[val] = slot_of[web_root(val)];
    free(free_at);
    free(taken);
    plan_free_at = NULL;

    /* The first pass's locals are all taken by now: what is left in reach
     * is what gen_local_fits says. The inlined calls' scratch, moved into
     * the frame, is in it too. */
    for (slot = 0; slot != nmerged; slot++)
        bytes += inlined[slot].size;
    /* Against the locals kept -- gen_local_fits counts from locals_size,
     * which the first pass left at all of them. */
    if (bytes && !gen_local_fits(bytes + locals_kept() - locals_size)) {
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
        for (val = 0; val != nvals; val++)
            if (web_root(val) == val && fixed_slot[val] == IY_SLOT)
                fixed_slot[val] = at;
    }
    for (val = 0; fixed_slot && val != nvals; val++)
        if (!vals[val].fwd && vals[val].reg == HOME_SLOT
            && fixed_slot[web_root(val)])
            vals[val].slot = fixed_slot[web_root(val)];
}

/* A block's live phis, which say whether an edge into it needs copies. */
static int has_phis(int blk)
{
    return phi_head[blk] >= 0;
}

/* The copies an edge makes into the phis of the block it goes to: every
 * value the edge brings loaded first and held -- in a register, or where
 * the backend puts one when it runs out -- and only then stored, the last
 * first, so that a phi read by another's copy is read before it changes. */
static void edge_copies(int from, int to)
{
    int nstored = 0, stored[MAX_LOCALS * 4], phi, pred = pred_index(to, from);

    if (pred >= 0) {
        for (phi = phi_head[to]; phi >= 0; phi = phi_next[phi]) {
            const Phi *join = &phis[phi];
            Ent ent;

            if (join->in[pred] < 0
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
static void leaf_edge(int from, int to);
static int  falls_to(int blk, int target);
static int  leaf_mode;          /* this function made by emit_leaf */
static int  leaf_iy_saved_at, leaf_bc_saved_at;    /* IY and BC across a
                                                     * call: see leaf_save_slots */
int  blocks_from;        /* where the function made here begins */

/* Slots for IY and BC across a call, where a value that lives across one
 * has either for its home: before the values' own, so as near the frame
 * pointer as the frame allows. */
static void leaf_save_slots(void)
{
    int val, iy = 0, bc = 0;

    for (val = 0; val != nvals; val++)
        if (vals[val].used && !vals[val].fwd && across_calls[val]) {
            if (vals[val].reg == R_BC)
                bc = 1;
            else if (iy_web >= 0 && fixed_slot[web_root(val)] == fixed_slot[iy_web])
                iy = 1;
        }
    leaf_iy_saved_at = iy ? gen_local(ACC_INT_SIZE) : 0;
    leaf_bc_saved_at = bc ? gen_local(ACC_INT_SIZE) : 0;
}

/* Which of IY and BC the call at `at`, in `blk`, has to keep: 1 for IY,
 * 2 for BC, where a value with that home is live after it -- the block's
 * live-out walked back to it, not counting what the call itself makes. */
static int *call_homes;          /* by instruction: what leaf_live_homes said, or -1 */
static int call_homes_n;

/* The homes the values live after each instruction of a block have, all of
 * them in one walk back from the block's end -- a count of the live values
 * in BC and of those in IY's web, kept as values come and go -- where a
 * walk from the end to each call was quadratic in a block of calls. */
static int home_bits(int val)
{
    if (vals[val].fwd)
        return 0;
    if (vals[val].reg == R_BC)
        return 2;
    if (iy_web >= 0 && fixed_slot[web_root(val)] == fixed_slot[iy_web])
        return 1;

    return 0;
}

static void block_homes(int blk)
{
    int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns, pos, operand;
    int nbc = 0, niy = 0, k;

    lset_from(&live_out, blk);
    for (k = 0; k != lset_n; k++) {
        int bits = home_bits(lset_mem[k]);

        nbc += bits == 2;
        niy += bits == 1;
    }
    for (pos = end - 1; pos >= blocks[blk].first; pos--) {
        const Ins *insn = &insns[pos];
        int made = insn->res >= 0 ? insn->res
                   : insn->op == I_SET ? insn->target : -1;
        int bc = nbc, iy = niy;

        /* What is live after it, less what it makes itself. */
        if (insn->res >= 0 && lset_pos[insn->res] >= 0) {
            int bits = home_bits(insn->res);

            bc -= bits == 2;
            iy -= bits == 1;
        }
        call_homes[pos] = (bc ? 2 : 0) | (iy ? 1 : 0);
        if (made >= 0 && lset_pos[made] >= 0) {
            int bits = home_bits(made);

            nbc -= bits == 2;
            niy -= bits == 1;
            lset_del(made);
        }
        for (operand = 0; operand != insn->nin; operand++) {
            int val = insn->in[operand].val;

            if (val >= 0 && lset_pos[val] < 0) {
                int bits = home_bits(val);

                nbc += bits == 2;
                niy += bits == 1;
                lset_add(val);
            }
        }
    }
    lset_clear();
}

/* Which of IY and BC the call at `at`, in `blk`, has to keep: 1 for IY,
 * 2 for BC, where a value with that home is live after it -- the block's
 * live-out walked back to it, not counting what the call itself makes. */
static int leaf_live_homes(int at, int blk)
{
    if (call_homes_n != ninsns) {
        int k;

        call_homes = realloc(call_homes, ((size_t) ninsns + 1) * sizeof *call_homes);
        if (!call_homes)
            acc_error("out of memory for the SSA form");
        for (k = 0; k != ninsns; k++)
            call_homes[k] = -1;
        call_homes_n = ninsns;
        lset_alloc();
    }
    if (call_homes[at] < 0)
        block_homes(blk);

    return call_homes[at];
}

/* IY and BC -- those of `homes` -- kept around a call, `back` set to put
 * them back after it: straight to and from their slots within (ix+d)'s
 * reach, and past it through HL before the call, which is free then, and
 * DE after. */
static void leaf_keep_homes(int homes, int back)
{
    if (!(homes & 1)) {
        /* IY is not live across it */
    } else if (disp_fits(leaf_iy_saved_at)) {
        frame_byte(back ? 0x31 : 0x3e, leaf_iy_saved_at);   /* ld iy, (ix+d) */
    } else if (!back) {
        out_byte2(0xfd, 0xe5);                  /* push iy */
        pop_rr(R_HL);
        ld_ix_rr(leaf_iy_saved_at, R_HL);
    } else {
        ld_rr_ix(R_DE, leaf_iy_saved_at);
        push_rr(R_DE);
        out_byte2(0xfd, 0xe1);                  /* pop iy */
    }
    if (homes & 2) {
        if (back)
            ld_rr_ix(R_BC, leaf_bc_saved_at);
        else
            ld_ix_rr(leaf_bc_saved_at, R_BC);
    }
}

/* The copies of the trampolines waiting, each then a jump on to its
 * block: made where the code before cannot fall into them. */
static void emit_trampolines(void)
{
    int tramp;

    for (tramp = 0; tramp != ntrampolines && !fail; tramp++) {
        int to = trampolines[tramp].to;

        gen_label(trampolines[tramp].hole);
        if (leaf_mode)
            leaf_edge(trampolines[tramp].from, to);
        else if (regs_on())
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

    /* A declaration logs a claim whatever it is, and only a `register`
     * one takes IY: one in memory, its address taken, takes nothing. */
    for (at = 1; at != ninsns; at++)
        if (insns[at].op == I_FRAME && !frame_of_value(&insns[at])
            && ((insns[at].rec->op == GL_gen_iy_claim
                 && ((int) insns[at].rec->arg[2] & SQ_REGISTER))
                || insns[at].rec->op == GL_gen_iy_param
                || insns[at].rec->op == GL_gen_iy_take))
            return 1;

    return 0;
}

/* Whether a frame call is about a local that is values now, and so is not
 * made: the local in IY is one of them. */
int frame_of_value(const Ins *insn)
{
    switch (insn->rec->op) {
    case GL_gen_iy_claim: case GL_gen_iy_param: case GL_gen_iy_take:
        return offset_all_values((int) insn->rec->arg[0]);
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
        gen_switch_load(inline_moved((int) insn->rec->arg[0]),
                        (Type) insn->rec->arg[1]);
        break;
    case GL_gen_switch_case:
        gen_switch_case(insn->rec->arg[0], (uint32_t) insn->rec->arg[1],
                        (Type) insn->rec->arg[2], block_now[insn->target],
                        inline_moved((int) insn->rec->arg[4]));
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
    emit_start();
    nmoved = 0;
    ntrampolines = 0;

    /* The prologue, and the frame as the first pass laid it out -- but
     * for the local in IY, if it is values now. */
    call(&insns[0]);
    nlocal_moves = 0;
    for (at = 1; at != ninsns; at++)
        if (insns[at].op == I_FRAME && !frame_of_value(&insns[at]))
            frame_again(&insns[at]);
    gen_local_settle();                 /* the values' slots live throughout */
    give_slots();
    inline_slots();

    /* Each parameter that is values now, from its slot into its first. */
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param) {
            int val = locals[local].entry_val;

            vpush_local(locals[local].offset, locals[local].type);
            vstore_local(vals[val].slot, vals[val].type);
            vdrop();
        }

    emit_raws();
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

/* Whether a value read from memory, and read lazily -- a local, or through
 * a pointer, which the classic backend reads where the value is used --
 * would wait on the classic stack across something that may write that
 * memory: a call, or a store. `int z = y; b(&y); c(z, y);` read y for z
 * after b had changed it. */
static int read_then_written(int def, int use)
{
    int op = insns[def].op, at;

    if (op != GL_vpush_local && op != GL_vderef && op != GL_vmember)
        return 0;
    for (at = def + 1; at < use; at++)
        switch (insns[at].op) {
        case GL_gen_call: case GL_gen_call_indirect: case GL_vstore_indirect:
        case GL_vstore_local: case GL_vprefix_indirect:
        case GL_vpostfix_indirect: case GL_vprefix_local:
        case GL_vpostfix_local: case GL_gen_copy_to_array:
        case GL_gen_zero_array:
            return 1;
        }

    return 0;
}

/* Whether a value is a parameter's, as the function began: in its slot or
 * the home it was loaded into, whatever happens on the stack. */
static int leaf_parameter(int val)
{
    int local;

    for (local = 0; val >= 0 && local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param
            && locals[local].entry_val == val)
            return 1;

    return 0;
}

/* With the code made here, whether a value is an argument of a call other
 * than its last that cannot wait on the stack for it: the arguments are
 * pushed last first, so one left on the stack would be under the ones after
 * it -- unless each of those is loaded where it is, through DE around it:
 * a constant, or a parameter as it came, of an int or the parameter's
 * own type, so that nothing converts it on the way. */
static int leaf_early_argument(int val, int use)
{
    const Ins *call = &insns[use];
    int operand, later, first, nparams;

    if (call->op != GL_gen_call)
        return 0;
    first = (int) call->rec->arg[2];
    nparams = (int) call->rec->arg[3];
    for (operand = 0; operand + 1 < call->nin; operand++) {
        if (call->in[operand].val != val)
            continue;
        for (later = operand + 1; later < call->nin; later++) {
            const Ent *ent = &call->in[later];
            Type type = later < nparams ? sym_param_type(first, later) : TY_INT;

            if (!(ent->val == S_CONST || leaf_parameter(ent->val)))
                return 1;
            if (type_size(type) != ACC_INT_SIZE
                && !(ent->val >= 0 && vals[ent->val].type == type))
                return 1;
        }
        return 0;
    }

    return 0;
}

/* Which values are left on the classic stack: read once, in the block that
 * makes them, and in stack order -- checked by walking each block with a
 * stack of its own, and a value that is not where the classic stack would
 * have it is taken out and the walk made again. */
static int leaf_long(Type type);
static int leaf_long_ok(int val);

static void find_forwarded(void)
{
    int *uses = calloc((size_t) nvals + 1, sizeof *uses);
    int *use_at = calloc((size_t) nvals + 1, sizeof *use_at);
    int *sim = malloc(((size_t) nvals + 1) * sizeof *sim);
    int val, at, operand, phi, pred, blk;

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
                        && !(leaf_mode && leaf_early_argument(val, use_at[val]))
                        && !read_then_written(def_at[val], use_at[val])
                        && use_at[val] > def_at[val]
                        && insns[use_at[val]].block == insns[def_at[val]].block
                        && !narrow_unwidened(val)
                        && !(leaf_mode && (type_wide(vals[val].type)
                                           || type_float(vals[val].type))
                             && !(leaf_long(vals[val].type) && leaf_long_ok(val)
                                  && !insns[def_at[val]].delegated));

    /* One walk a block. A value taken out is left where it is on the
     * stack and skipped: every value made after it is above it, so taking
     * it out moves none of what the instructions between read -- the walk
     * made again would find what this one goes on to find. */
    for (blk = 0; blk != nblocks; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;
        int nsim = 0;

        for (at = blocks[blk].first; at != end; at++) {
            const Ins *insn = &insns[at];
            int want = 0, rank, top, kept, ok = 1;

            for (operand = 0; operand != insn->nin; operand++) {
                val = insn->in[operand].val;
                if (val >= 0 && vals[val].fwd)
                    want++;
            }

            /* The `want` topmost still forwarded, the out-taken among them
             * dropped: each is walked over once. */
            top = nsim;
            kept = 0;
            while (top && kept != want) {
                top--;
                if (vals[sim[top]].fwd)
                    sim[nsim - ++kept] = sim[top];
            }
            if (kept != want) {
                ok = 0;
            } else {
                rank = 0;
                for (operand = 0; operand != insn->nin; operand++) {
                    val = insn->in[operand].val;
                    if (val < 0 || !vals[val].fwd)
                        continue;
                    if (sim[nsim - want + rank] != val)
                        ok = 0;
                    rank++;
                }
            }
            memmove(&sim[top], &sim[nsim - kept], (size_t) kept * sizeof *sim);
            nsim = top + kept;
            if (!ok) {
                for (operand = 0; operand != insn->nin; operand++)
                    if (insn->in[operand].val >= 0)
                        vals[insn->in[operand].val].fwd = 0;
            } else {
                nsim -= want;
            }
            if (insn->res >= 0 && vals[insn->res].fwd)
                sim[nsim++] = insn->res;
        }
        while (nsim)
            vals[sim[--nsim]].fwd = 0;
    }
    free(uses);
    free(use_at);
    free(sim);
}

/* Whether a value can live in a register: a scalar an int or narrower,
 * read nowhere as a bit-field. */
static int reg_eligible(int val)
{
    Type type = vals[val].type;

    if (vals[val].fwd || !vals[val].used || vals[val].mem || type == TY_VOID
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

    if (leaf_mode)
        return 1;                       /* DE is the code's own */

    return homes && *homes == '2' ? 2 : homes && *homes == '0' ? 0 : 1;
}

/* Which values are live at once, and so cannot share a home: a value
 * clashes with every value live just after the instruction that makes it
 * -- after, so that one read last there can give its home to the result --
 * and the phis and parameters, made at the top of a block, with every value
 * live there. Found walking each block back from what is live out of it,
 * which also finds each operand that is the last read of its value.
 *
 * The live values as a sparse set -- added, taken out and walked each in
 * time of its own -- and the clashes as a sorted list a value: the work is
 * the clashes found, which the work bound keeps linear. */
static Pairs clash_pairs;               /* blk: one value, val: the other */

static void clash(int one, int two)
{
    if (one == two)
        return;
    pairs_add(&clash_pairs, one, two);
}

/* Each value made at the top of `blk` clashes with what is live there and
 * with the others made there. */
static void clash_at_top(int blk)
{
    int made[MAX_LOCALS * 4], nmade = 0, phi, local, val, at;

    for (phi = phi_head[blk]; phi >= 0; phi = phi_next[phi])
        if (nmade != (int) (sizeof made / sizeof made[0]))
            made[nmade++] = phis[phi].val;
    for (local = 0; blk == 0 && local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param
            && nmade != (int) (sizeof made / sizeof made[0]))
            made[nmade++] = locals[local].entry_val;
    for (at = 0; at != nmade; at++) {
        work(lset_n + at);
        for (val = 0; val != lset_n; val++)
            clash(made[at], lset_mem[val]);
        for (val = 0; val != at; val++)
            clash(made[at], made[val]);
    }
}

/* The pairs found made the clash lists: both ways round, each sorted,
 * each pair once. */
static void clashes_make(void)
{
    int k, val, *fill;

    free(clash_off);
    free(clash_adj);
    clash_off = calloc((size_t) nvals + 2, sizeof *clash_off);
    clash_adj = malloc(((size_t) clash_pairs.n * 2 + 1) * sizeof *clash_adj);
    fill = malloc(((size_t) nvals + 1) * sizeof *fill);
    if (!clash_off || !clash_adj || !fill)
        acc_error("out of memory for the SSA form");
    for (k = 0; k != clash_pairs.n; k++) {
        clash_off[clash_pairs.blk[k] + 1]++;
        clash_off[clash_pairs.val[k] + 1]++;
    }
    for (val = 0; val != nvals; val++)
        clash_off[val + 1] += clash_off[val];
    memcpy(fill, clash_off, (size_t) nvals * sizeof *fill);
    for (k = 0; k != clash_pairs.n; k++) {
        clash_adj[fill[clash_pairs.blk[k]]++] = clash_pairs.val[k];
        clash_adj[fill[clash_pairs.val[k]]++] = clash_pairs.blk[k];
    }
    /* Sorted, and each value's run of the same one made one. */
    for (val = 0; val != nvals; val++) {
        int from = clash_off[val], to = clash_off[val + 1], put = from;

        qsort(clash_adj + from, (size_t) (to - from), sizeof *clash_adj, by_int);
        for (k = from; k != to; k++)
            if (k == from || clash_adj[k] != clash_adj[k - 1])
                clash_adj[put++] = clash_adj[k];
        fill[val] = put;
    }
    /* Squeezed up, the runs being shorter now. */
    {
        int put = 0, start;

        for (val = 0; val != nvals; val++) {
            start = clash_off[val];
            clash_off[val] = put;
            for (k = start; k != fill[val]; k++)
                clash_adj[put++] = clash_adj[k];
        }
        clash_off[nvals] = put;
    }
    free(fill);
    clash_pairs.n = 0;
}

static void find_clashes(void)
{
    Pairs tops = { 0 };
    int blk;

    find_loops();
    across_calls = realloc(across_calls, ((size_t) nvals + 1) * sizeof *across_calls);
    if (!across_calls)
        acc_error("out of memory for the SSA form");
    memset(across_calls, 0, ((size_t) nvals + 1) * sizeof *across_calls);
    lset_alloc();
    clash_pairs.n = 0;
    for (blk = 0; blk != nblocks && !fail; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns, at, k;

        lset_from(&live_out, blk);
        for (at = end - 1; at >= blocks[blk].first; at--) {
            Ins *insn = &insns[at];
            int made = insn->res >= 0 ? insn->res
                       : insn->op == I_SET ? insn->target : -1, val, operand;

            insn->kills = 0;
            if (made >= 0) {
                lset_del(made);
                if (!work(lset_n))
                    break;
                for (val = 0; val != lset_n; val++)
                    clash(made, lset_mem[val]);
            }

            /* What a call leaves live after it had to be kept across it. */
            if (insn->op == GL_gen_call || insn->op == GL_gen_call_indirect) {
                int depth = loop_depth[blk];

                if (!work(lset_n))
                    break;
                for (val = 0; val != lset_n; val++)
                    across_calls[lset_mem[val]] += 1L << (3 * (depth > 5 ? 5 : depth));
            }
            for (operand = insn->nin - 1; operand >= 0; operand--) {
                val = insn->in[operand].val;
                if (val < 0 || lset_pos[val] >= 0)
                    continue;
                insn->kills |= (unsigned char) (1 << operand);
                lset_add(val);
            }
        }
        clash_at_top(blk);
        for (k = 0; k != lset_n; k++)
            pairs_add(&tops, blk, lset_mem[k]);
    }
    lset_clear();
    bset_make(&live_top, tops.blk, tops.val, tops.n);
    free(tops.blk);
    free(tops.val);
    clashes_make();
}

/* Webs: a phi and the values that come into it joined, where their
 * intervals allow, so that they have one home and the copies between them
 * are no copies. A union-find, and each web's values in a list from its
 * root through web_next. */
static int *web_parent, *web_tail;
static long *web_weight, *web_gain;
static long *web_clashes;               /* by root: its values' clash lists */

static int web_root(int val)
{
    while (web_parent[val] != val)
        val = web_parent[val] = web_parent[web_parent[val]];

    return val;
}

/* Whether two webs, by their roots, clash: a value of one clashing with a
 * value of the other -- found through the clash lists of the one with the
 * shorter, the time those take, not through every pair. */
static int webs_clash(int one, int two)
{
    int left, k, root;

    if (web_clashes[one] > web_clashes[two]) {
        left = one;
        one = two;
        two = left;
    }
    if (!work(web_clashes[one]))
        return 1;
    root = web_root(two);
    for (left = one; left >= 0; left = web_next[left])
        for (k = clash_off[left]; k != clash_off[left + 1]; k++)
            if (web_root(clash_adj[k]) == root)
                return 1;

    return 0;
}

/* `from`'s web, by its root, made part of `into`'s. */
static void webs_join(int into, int from)
{
    web_parent[from] = into;
    web_next[web_tail[into]] = from;
    web_tail[into] = web_tail[from];
    web_clashes[into] += web_clashes[from];
}

/* How deep in loops each block is: a jump to a block that dominates the
 * one it is made from closes a loop, whose body is what reaches the jump
 * without going through the block it goes to. Whether one block dominates
 * another is asked of the dominator tree numbered in a walk, the one's
 * span holding the other's number; and each body's blocks are listed, to
 * be counted and cleared without looking at the rest. */
static int *dom_pre, *dom_post;

static void dom_number(void)
{
    int *stack = malloc(((size_t) nblocks + 1) * sizeof *stack);
    int *next_kid = calloc((size_t) nblocks + 1, sizeof *next_kid);
    int depth = 0, clock = 0;

    dom_pre = realloc(dom_pre, ((size_t) nblocks + 1) * sizeof *dom_pre);
    dom_post = realloc(dom_post, ((size_t) nblocks + 1) * sizeof *dom_post);
    if (!stack || !next_kid || !dom_pre || !dom_post)
        acc_error("out of memory for the SSA form");
    stack[depth++] = 0;
    dom_pre[0] = clock++;
    while (depth) {
        int blk = stack[depth - 1];

        if (next_kid[blk] == dom_kids[blk].count) {
            dom_post[blk] = clock++;
            depth--;
            continue;
        }
        blk = dom_kids[blk].at[next_kid[blk]++];
        dom_pre[blk] = clock++;
        stack[depth++] = blk;
    }
    free(stack);
    free(next_kid);
}

static int dominates(int over, int blk)
{
    return dom_pre[over] <= dom_pre[blk] && dom_post[blk] <= dom_post[over];
}

void find_loops(void)
{
    int *work_list = malloc(((size_t) nblocks + 1) * sizeof *work_list);
    int *body_list = malloc(((size_t) nblocks + 1) * sizeof *body_list);
    unsigned char *in_loop = calloc((size_t) nblocks + 1, 1);
    int blk, succ;

    loop_depth = realloc(loop_depth, ((size_t) nblocks + 1) * sizeof *loop_depth);
    if (!work_list || !body_list || !in_loop || !loop_depth)
        acc_error("out of memory for the SSA form");
    dom_number();
    for (blk = 0; blk != nblocks; blk++)
        loop_depth[blk] = 0;
    for (blk = 0; blk != nblocks; blk++) {
        if (rpo_num[blk] < 0)
            continue;
        for (succ = 0; succ != succs[blk].count; succ++) {
            int head = succs[blk].at[succ], nwork = 0, nbody = 0, at;

            if (rpo_num[head] < 0 || !dominates(head, blk))
                continue;
            in_loop[head] = 1;
            body_list[nbody++] = head;
            if (!in_loop[blk]) {
                in_loop[blk] = 1;
                body_list[nbody++] = blk;
                work_list[nwork++] = blk;
            }
            while (nwork) {
                int body = work_list[--nwork], pred;

                for (pred = 0; pred != preds[body].count; pred++) {
                    int from = preds[body].at[pred];

                    if (!in_loop[from] && rpo_num[from] >= 0) {
                        in_loop[from] = 1;
                        body_list[nbody++] = from;
                        work_list[nwork++] = from;
                    }
                }
            }
            work(nbody);
            for (at = 0; at != nbody; at++) {
                loop_depth[body_list[at]]++;
                in_loop[body_list[at]] = 0;
            }
        }
    }
    free(work_list);
    free(body_list);
    free(in_loop);
}

/* Whether the code here is likely to select an instruction itself, with
 * its operands where they live: see native_compare and the rest. */
static int selected_here(const Ins *insn)
{
    int op = insn->op == GL_vapply ? (int) insn->rec->arg[0] : 0;

    if (insn->op == I_STEP)
        return native_on();
    if (insn->op != GL_vapply || !native_on())
        return 0;

    return op == TK_PLUS || op == TK_MINUS || op == TK_LT || op == TK_GT
           || op == TK_LE || op == TK_GE || op == TK_EQ || op == TK_NE;
}

/* Each value tried against every later one of its local, in order. A web
 * tried against them all once needs no trying again, from any of its
 * values: each later one joined it or clashed with it, and a web that
 * grows clashes with whatever it clashed with before. So each value is
 * tried only from a web not tried yet. */
static void coalesce_cached(void)
{
    int *count = calloc((size_t) nlocals + 2, sizeof *count);
    int *of_local = malloc(((size_t) nvals + 1) * sizeof *of_local);
    char *tried = calloc((size_t) nvals + 1, 1);
    int val, local, at, other;

    if (!count || !of_local || !tried)
        acc_error("out of memory for the SSA form");
    for (val = 0; val != nvals; val++)
        if (vals[val].of_cached && !vals[val].fwd && vals[val].used)
            count[vals[val].of_cached + 1]++;
    for (local = 1; local <= nlocals; local++)
        count[local + 1] += count[local];
    for (val = 0; val != nvals; val++)
        if (vals[val].of_cached && !vals[val].fwd && vals[val].used)
            of_local[count[vals[val].of_cached]++] = val;

    /* count[local] is now where local + 1's values begin. */
    for (local = 1; local <= nlocals && !fail; local++) {
        int begin = count[local - 1], end = count[local];

        for (at = begin; at != end && !fail; at++) {
            int into = web_root(of_local[at]);

            if (tried[into])
                continue;
            work(end - at);
            for (other = at + 1; other != end; other++) {
                int from = web_root(of_local[other]);

                if (vals[of_local[other]].type != vals[of_local[at]].type
                    || into == from || webs_clash(into, from))
                    continue;
                webs_join(into, from);
            }
            tried[into] = 1;
        }
        for (at = begin; at != end; at++)
            tried[web_root(of_local[at])] = 0;
    }
    free(count);
    free(of_local);
    free(tried);
}

static void coalesce(void)
{
    size_t size = ((size_t) nvals + 1) * sizeof *web_parent;
    int val, phi, pred, at, operand;

    web_parent = realloc(web_parent, size);
    web_next = realloc(web_next, size);
    web_tail = realloc(web_tail, size);
    web_weight = realloc(web_weight, ((size_t) nvals + 1) * sizeof *web_weight);
    web_gain = realloc(web_gain, ((size_t) nvals + 1) * sizeof *web_gain);
    web_clashes = realloc(web_clashes, ((size_t) nvals + 1) * sizeof *web_clashes);
    if (!web_parent || !web_next || !web_tail || !web_weight || !web_gain
        || !web_clashes)
        acc_error("out of memory for the SSA form");
    for (val = 0; val != nvals; val++) {
        web_parent[val] = val;
        web_next[val] = -1;
        web_tail[val] = val;
        web_weight[val] = 0;
        web_gain[val] = 0;
        web_clashes[val] = clash_off ? clash_off[val + 1] - clash_off[val] : 0;
    }
    if (!regs_on())
        return;

    for (phi = 0; phi != nphis; phi++) {
        const Phi *join = &phis[phi];

        if (!join->live)
            continue;
        for (pred = 0; pred != preds[join->block].count; pred++) {
            int source = join->in[pred], into, from;

            if (source < 0 || vals[source].fwd || vals[source].mem
                || vals[source].type != vals[join->val].type)
                continue;
            into = web_root(join->val);
            from = web_root(source);
            if (into == from || webs_clash(into, from))
                continue;
            webs_join(into, from);
        }
    }

    /* A cached local's values, all of them its own value at some point,
     * in one home where none clashes: the reload after a call then lands
     * where the value was before it. */
    coalesce_cached();

    /* Each read, and each making, eight times over for each loop around
     * it: what a register saves is paid where the value is used. The
     * weight says which web IY is worth most to; the gain says whether a
     * register is worth anything at all -- a read the code here selects
     * uses the register where it is, one gen.h makes copies it out with a
     * push and a pop, which costs more than a load from the frame, and a
     * call it lives across spills and reloads it. */
    for (at = 0; at != ninsns; at++) {
        int depth = loop_depth[insns[at].block];
        long weight = 1L << (3 * (depth > 5 ? 5 : depth));

        for (operand = 0; operand != insns[at].nin; operand++) {
            int val = insns[at].in[operand].val;

            if (val < 0)
                continue;
            web_weight[web_root(val)] += weight;
            web_gain[web_root(val)] += weight * (selected_here(&insns[at]) ? 4 : -2);
        }
        if (insns[at].res >= 0) {
            web_weight[web_root(insns[at].res)] += weight;
            web_gain[web_root(insns[at].res)] -= weight;
        }
    }
    for (at = 0; at != nvals; at++)
        web_gain[web_root(at)] -= 12 * across_calls[at];
}

static int by_weight(const void *left, const void *right)
{
    int lval = *(const int *) left, rval = *(const int *) right;

    if (web_weight[lval] != web_weight[rval])
        return web_weight[lval] > web_weight[rval] ? -1 : 1;

    return lval < rval ? -1 : lval > rval;
}

static int by_gain(const void *left, const void *right)
{
    int lval = *(const int *) left, rval = *(const int *) right;

    if (web_gain[lval] != web_gain[rval])
        return web_gain[lval] > web_gain[rval] ? -1 : 1;

    return lval < rval ? -1 : lval > rval;
}

static int iy_taken_already(void);
static int *switch_cases;       /* by instruction: the switch cases before it */

/* Webs whose slots are decided before the rest: the heaviest web left in
 * the frame that IY can hold goes in IY, if the first pass left IY free;
 * and a web holding a parameter's first value keeps to the parameter's
 * own slot, which nothing else reads now. */
static int *iy_roots, niy_roots, iy_roots_cap;     /* the webs in IY, in order */

static void plan_fixed(const int *order, int nroots)
{
    int at, local, taken;
    char *in_iy = calloc((size_t) nvals + 1, 1);
    char *far_param = calloc((size_t) nvals + 1, 1);

    fixed_slot = realloc(fixed_slot, ((size_t) nvals + 1) * sizeof *fixed_slot);
    if (!fixed_slot || !in_iy || !far_param)
        acc_error("out of memory for the SSA form");
    memset(fixed_slot, 0, ((size_t) nvals + 1) * sizeof *fixed_slot);

    /* The webs holding a parameter out of (ix+d)'s reach, by root. */
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param
            && !disp_fits(locals[local].offset))
            far_param[web_root(locals[local].entry_val)] = 1;
    iy_web = -1;
    niy_roots = 0;
    taken = iy_taken_already();         /* once: it looks at every frame call */
    for (at = 0; at != nroots && !taken && getenv("OPTACC_IY"); at++) {
        int root = order[at], member, fits = 1;

        long across = 0;

        for (member = root; member >= 0; member = web_next[member]) {
            if (vals[member].fwd || vals[member].reg != HOME_SLOT
                || !reg_eligible(member) || !gen_iy_can(vals[member].type)
                || type_size(vals[member].type) != ACC_INT_SIZE)
                fits = 0;           /* a narrow one is widened at each read */
            across += across_calls[member];
        }

        /* A call does not keep IY: the leaf backend saves it in a slot
         * around each, which is worth it where it is read more often than
         * it is saved. */
        if (leaf_mode && across && 2 * across > web_weight[root])
            fits = 0;

        /* Nor a parameter out of (ix+d)'s reach: the classic backend
         * reaches one through IY, which it cannot then load into IY. */
        if (far_param[root])
            fits = 0;
        if (!fits)
            continue;

        /* For the code made here, IY is shared as BC is: by every web
         * that fits and is live nowhere another in IY is -- the first, the
         * heaviest, named iy_web. */
        if (iy_web >= 0) {
            int left, k, clash = 0;

            if (!leaf_mode)
                break;
            work(web_clashes[root]);
            for (left = root; left >= 0 && !clash; left = web_next[left])
                for (k = clash_off[left]; k != clash_off[left + 1]; k++)
                    if (in_iy[web_root(clash_adj[k])])
                        clash = 1;
            if (clash)
                continue;
        } else {
            iy_web = root;
        }
        in_iy[root] = 1;
        fixed_slot[root] = IY_SLOT;     /* made in give_slots */
        GROW(iy_roots, niy_roots, iy_roots_cap);
        iy_roots[niy_roots++] = root;
    }
    for (local = 0; local != nlocals; local++) {
        int root;

        if (!locals[local].ok || !locals[local].is_param)
            continue;
        root = web_root(locals[local].entry_val);
        if (!fixed_slot[root] && vals[root].reg == HOME_SLOT && !vals[root].fwd)
            fixed_slot[root] = locals[local].offset;
    }

    /* A cached local's memory is its own slot. */
    for (local = 0; local != nlocals; local++)
        if (locals[local].cached)
            fixed_slot[web_root(locals[local].mem_val)] = locals[local].offset;
    free(in_iy);
    free(far_param);
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
    qsort(order, (size_t) nroots, sizeof *order, by_gain);

    /* How many switch cases come before each instruction. */
    switch_cases = realloc(switch_cases, ((size_t) ninsns + 2) * sizeof *switch_cases);
    if (!switch_cases)
        acc_error("out of memory for the SSA form");
    switch_cases[0] = 0;
    for (at = 0; at != ninsns; at++)
        switch_cases[at + 1] = switch_cases[at] + (insns[at].op == GL_gen_switch_case);

    for (at = 0; at != nroots; at++) {
        int root = order[at], homes = homes_wanted(), member, fits = 1;

        if (web_gain[root] <= 0 && !getenv("OPTACC_ANY_GAIN"))
            break;                      /* no better off in a register --
                                         * OPTACC_ANY_GAIN gives it one
                                         * anyway, for the tests */

        for (member = root; member >= 0 && fits; member = web_next[member]) {
            if (vals[member].reg == -2 || !reg_eligible(member))
                fits = 0;

            /* A switch's tests load DE: a value across them is in BC --
             * counted from a running count of them, not looked for. */
            if (fits && switch_cases[vals[member].last + 1] - switch_cases[vals[member].def])
                homes = 1;
        }
        for (reg = 0; reg != homes && fits; reg++) {
            int clash = 0, k;

            /* A clash with a value given the register already: one in a
             * member's clash list that has it. */
            for (member = root; member >= 0 && !clash; member = web_next[member])
                for (k = clash_off[member]; k != clash_off[member + 1] && !clash; k++)
                    clash = vals[clash_adj[k]].reg == home_regs[reg];
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
    qsort(order, (size_t) nroots, sizeof *order, by_weight);
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
    int val, k;

    for (k = live_top.off[blk]; k != live_top.off[blk + 1] && !fail; k++) {
        val = live_top.val[k];
        if (vals[val].reg != HOME_SLOT)
            pin_add(val);
    }
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

    if (val < 0 || vals[val].reg == HOME_SLOT) {
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
        && (vsp - 1)->kind == VAL_REG
        && !(type_unsigned(vals[val].type)
             && vwidth(vsp - 1) <= type_size(vals[val].type))) {
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
    int pred, phi, val, move, k;

    for (k = live_in.off[to]; k != live_in.off[to + 1] && !fail; k++) {
        val = live_in.val[k];
        if (vals[val].reg != HOME_SLOT)
            pin_add(val);
    }

    /* And a phi's value already in the phi's register, which needs no
     * copy but must not be taken for scratch by the others. */
    pred = pred_index(to, from);
    if (pred >= 0) {
        for (phi = phi_head[to]; phi >= 0 && !fail; phi = phi_next[phi]) {
            const Phi *join = &phis[phi];
            int source;

            source = join->in[pred];
            if (source >= 0 && vals[source].reg != HOME_SLOT
                && vals[source].reg == vals[join->val].reg
                && pin_index(source) < 0)
                pin_add(source);
        }
    }

    if (pred >= 0) {
        for (phi = phi_head[to]; phi >= 0 && !fail; phi = phi_next[phi]) {
            const Phi *join = &phis[phi];
            int source, again = 0;

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

/* Whether the edge from `from` to `to` has anything to copy: a phi whose
 * value comes from somewhere other than its own home. */
static int edge_copies_any(int from, int to)
{
    int pred = pred_index(to, from), phi;

    if (pred >= 0) {
        for (phi = phi_head[to]; phi >= 0; phi = phi_next[phi]) {
            const Phi *join = &phis[phi];
            int source;

            source = join->in[pred];
            if (source < 0 || source == join->val)
                continue;
            if (vals[source].reg != vals[join->val].reg
                || (vals[source].reg == HOME_SLOT
                    && vals[source].slot != vals[join->val].slot))
                return 1;
        }
    }

    return 0;
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
    invert = edge_copies_any(blk, target) || block_now[target] < 0 ? insn->sense
                                                       : !insn->sense;
    if (invert)
        vtruth(TK_EQ);
    pins_restore();
    if (vtop != npins + 1) {
        fail = "internal: values left under a branch";
        return;
    }
    pins_hide();
    if (edge_copies_any(blk, target)) {
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

/* ------------------------------------------------------------------ */
/* code selected here                                                  */

/* With OPTACC_NATIVE, the instructions the classic backend makes worst of
 * with a value in a register are selected here: a comparison and the branch
 * on it, an add or a subtract, a step. The register is used where it is --
 * add hl, bc -- where the classic backend, which does not own it, can only
 * copy it to HL first with a push and a pop. HL, DE and A are the scratch;
 * nothing else is written but the result's home. An instruction whose
 * operands are not where this can reach them is left to gen.h. */

static int native_on(void)
{
    return getenv("OPTACC_NATIVE") != NULL;
}

enum { AT_NONE, AT_CONST, AT_STACK, AT_REG, AT_SLOT };

static int skip_branch = -1;    /* the branch selected with its comparison */
static int fused_from = -1;     /* and what made it, the rest between made too */
static int leaf_jump_left_out;  /* the block's jump left out: it falls on */


/* Where an int-wide operand is, and the register or displacement in
 * `*where`; AT_NONE when it is not something the code here handles. */
static int operand_place(const Ent *ent, int *where)
{
    Type type = ent->attr.type;
    int val = ent->val, pin;

    if (type_size(type) != ACC_INT_SIZE || type_wide(type) || type_float(type)
        || type_is_struct(type) || ent->attr.bits)
        return AT_NONE;
    if (val == S_CONST)
        return AT_CONST;
    if (val < 0 || type_size(vals[val].type) != ACC_INT_SIZE
        || type_float(vals[val].type))
        return AT_NONE;
    if (vals[val].fwd)
        return AT_STACK;
    if (vals[val].reg == HOME_SLOT) {
        *where = vals[val].slot;
        return AT_SLOT;
    }
    pin = pin_index(val);
    if (pin < 0 || vstack[pin].kind != VAL_REG
        || vstack[pin].val != vals[val].reg)
        return AT_NONE;
    *where = vals[val].reg;

    return AT_REG;
}

/* Whether the code here can be made now: the pins each in its home, and
 * this instruction's operands, at most two, on top of the classic stack.
 * Values waiting there under them for later instructions are moved out of
 * HL and DE, the scratch, to slots of their own, as the classic backend
 * moves one out of a register it wants -- the last thing asked, since it
 * writes code. Not with a byte waiting in A, or the local in IY read
 * lazily, which the code here may change under them. */
static int native_ready(const Ins *insn)
{
    int nfwd = 0, operand, pin, at;

    for (operand = 0; operand != insn->nin; operand++)
        if (insn->in[operand].val >= 0 && vals[insn->in[operand].val].fwd)
            nfwd++;
    if (nfwd > 2 || vtop < npins + nfwd)
        return 0;
    for (pin = 0; pin != npins; pin++)
        if (vstack[pin].kind != VAL_REG || vstack[pin].val != vals[pin_val[pin]].reg)
            return 0;
    for (at = npins; at != vtop - nfwd; at++)
        if (vstack[at].kind == VAL_ACC || vstack[at].kind == VAL_IY
            || vstack[at].kind == VAL_IYADDR)
            return 0;
    for (at = npins; at != vtop - nfwd; at++)
        if (vstack[at].kind == VAL_REG
            && (vstack[at].val == R_HL || vstack[at].val == R_DE)) {
            int slot = spill_slot();

            ld_ix_rr(slot, vstack[at].val);
            vstack[at].kind = VAL_LOCAL;
            vstack[at].val = slot;
        }

    return 1;
}

static int reg_taken(int reg)
{
    int pin;

    for (pin = 0; pin != npins; pin++)
        if (vals[pin_val[pin]].reg == reg)
            return 1;

    return 0;
}

/* An operand into `reg`, from wherever it is. */
static void load_into(const Ent *ent, int place, int where, int reg)
{
    switch (place) {
    case AT_CONST:
        load_constant(ent);
        force_into(vsp - 1, reg);
        vdrop();
        break;
    case AT_STACK:
        force_into(vsp - 1, reg);
        vdrop();
        break;
    case AT_REG:
        if (where != reg) {
            push_rr(where);
            pop_rr(reg);
        }
        break;
    case AT_SLOT:
        if (iy_local && where == iy_local)
            lea_rr_iy(reg, 0);
        else
            ld_rr_ix(reg, where);
        break;
    }
}

/* The value `val`, just made in HL, to where it lives. The pins of what
 * the instruction read last are let go of before this. */
static void result_in_hl(int val)
{
    if (vals[val].fwd) {
        vpush(VAL_REG, vals[val].type, R_HL);
        return;
    }
    if (!vals[val].used)
        return;
    if (vals[val].reg == HOME_SLOT) {
        if (iy_local && vals[val].slot == iy_local) {
            push_rr(R_HL);
            out_byte2(0xfd, 0xe1);      /* pop iy */
        } else {
            ld_ix_rr(vals[val].slot, R_HL);
        }
        return;
    }
    if (vals[val].reg == R_DE) {
        ex_de_hl();
    } else {
        push_rr(R_HL);
        pop_rr(vals[val].reg);
    }
    pin_add(val);
}

static int const_number(const Ent *ent, int *number)
{
    if (ent->val != S_CONST || ent->attr.kind != VAL_CONST)
        return 0;
    *number = ent->attr.val;

    return 1;
}

/* inc rr or dec rr, `count` times, for BC, DE, HL or (-1) IY. */
static void step_reg(int reg, int count)
{
    static const unsigned char inc_code[NREGS] = { 0x23, 0x13, 0x03 };
    int dec = count < 0;

    for (count = dec ? -count : count; count; count--) {
        if (reg < 0)
            out_byte2(0xfd, dec ? 0x2b : 0x23);         /* inc iy / dec iy */
        else
            out_byte(inc_code[reg] + (dec ? 8 : 0));
    }
}

static int mirror(int op)
{
    switch (op) {
    case TK_LT: return TK_GT;
    case TK_GT: return TK_LT;
    case TK_LE: return TK_GE;
    case TK_GE: return TK_LE;
    }

    return op;
}

/* The branch after the comparison at `at`, on the flags it left: jp on `cc`
 * when the comparison is true -- turned over when the branch jumps on false
 * -- to its block, or to the copies on the way there. */
static void branch_to(int cc, const Ins *branch, int blk)
{
    int target = branch->target;

    if (!branch->sense)
        cc ^= 0x08;                     /* the opposite condition */
    if (edge_copies_any(blk, target)) {
        GROW(trampolines, ntrampolines, trampolines_cap);
        trampolines[ntrampolines].hole = jump_op(cc);
        trampolines[ntrampolines].from = blk;
        trampolines[ntrampolines].to = target;
        ntrampolines++;
    } else if (block_now[target] >= 0) {
        gen_jump_cc_to(cc, block_now[target]);
    } else {
        jump_forward(jump_op(cc), target);
    }
}

static void branch_on(int cc, const Ins *branch, int blk, int at)
{
    branch_to(cc, branch, blk);
    skip_branch = at + 1;
}

/* An unsigned char in a register compared with 0, by the branch after it:
 * its byte into A and or a, the bytes above it being 0. */
static int native_byte_zero(const Ins *insn, int blk, int at)
{
    const Ins *branch = &insns[at + 1];
    int op = (int) insn->rec->arg[0], val = insn->in[0].val, number, pin;

    if ((op != TK_EQ && op != TK_NE) || insn->nin != 2 || val < 0
        || vals[val].type != TY_UCHAR || vals[val].reg == HOME_SLOT
        || !const_number(&insn->in[1], &number) || number != 0
        || insn->res < 0 || !vals[insn->res].fwd || at + 1 >= ninsns
        || branch->op != I_BR || branch->nin != 1
        || branch->in[0].val != insn->res || branch->target < 0
        || !native_ready(insn))
        return 0;
    pin = pin_index(val);
    if (pin < 0 || vstack[pin].kind != VAL_REG || vstack[pin].val != vals[val].reg)
        return 0;
    out_byte(vals[val].reg == R_BC ? 0x79 : 0x7b);     /* ld a, c or ld a, e */
    or_a_a();
    branch_on(op == TK_EQ ? JP_Z : JP_NZ, branch, blk, at);

    return 1;
}

/* After HL less a side, as signed: the sign of the difference into the
 * carry, and the carry turned over where it overflowed -- add hl, hl
 * leaves P/V as the subtract set it. The carry then says less, as for an
 * unsigned subtract: 6 bytes, and no register but HL. */
static void signed_carry(void)
{
    int over;

    add_hl_rr(R_HL);
    over = jump_op(0xe2);                       /* jp po */
    out_byte(0x3f);                             /* ccf */
    patch_to_here(over);
}

/* The comparison as native_compare's quick forms cannot make it: DE is
 * borrowed if it holds a value -- pushed first and popped last, which
 * leaves the flags -- the right side read and kept on the stack, the left
 * into HL, and HL less DE. Everything
 * is read before DE is written, so a side in DE is read where it is. A
 * side left on the classic stack is read first, since the others' loads
 * go through HL; two left there are the top and the one under it. */
static int compare_general(const Ins *insn, int op, int is_signed,
                           const int *place, const int *where)
{
    int borrow = reg_taken(R_DE), left = 0, right = 1;

    /* Whichever is on top of the classic stack is read first, as the
     * right side: the comparison turned about if that is the left. */
    if (place[0] == AT_STACK && place[1] != AT_STACK) {
        left = 1;
        right = 0;
        op = mirror(op);
    }
    if (borrow)
        push_rr(R_DE);
    load_into(&insn->in[right], place[right], where[right], R_HL);
    push_rr(R_HL);
    load_into(&insn->in[left], place[left], where[left], R_HL);
    pop_rr(R_DE);
    if (op == TK_LE || op == TK_GT)
        out_byte(0x37);                 /* scf: HL - DE - 1 */
    else
        or_a_a();
    sbc_hl_rr(R_DE);
    if (is_signed)
        signed_carry();
    if (borrow)
        pop_rr(R_DE);

    return op;
}

/* A comparison of two ints read only by the branch after it: the flags of
 * one subtraction, and a jump on them. HL is one side and the other is
 * where it is, in BC or DE, or loaded into DE. A signed comparison moves
 * both sides by 0x800000 first, which makes it an unsigned one. */
static int native_compare(const Ins *insn, int blk, int at)
{
    const Ins *branch = &insns[at + 1];
    int op = (int) insn->rec->arg[0], place[2], where[2] = { 0, 0 };
    int in_hl, other, is_signed, cc, number, carry = 0;

    if (at + 1 >= ninsns || insn->res < 0 || !vals[insn->res].fwd
        || branch->op != I_BR || branch->nin != 1
        || branch->in[0].val != insn->res || branch->target < 0
        || insn->nin != 2 || insn->rec->arg[1])
        return 0;
    if (op != TK_LT && op != TK_GT && op != TK_LE && op != TK_GE
        && op != TK_EQ && op != TK_NE)
        return 0;
    place[0] = operand_place(&insn->in[0], &where[0]);
    place[1] = operand_place(&insn->in[1], &where[1]);
    if (place[0] == AT_NONE || place[1] == AT_NONE || !native_ready(insn))
        return 0;
    is_signed = !type_unsigned(insn->in[0].attr.type)
                && !type_unsigned(insn->in[1].attr.type)
                && op != TK_EQ && op != TK_NE;

    /* The side to subtract: one in a register where it is, if there is
     * one, and the right side otherwise. */
    other = place[0] == AT_REG && place[1] != AT_REG ? 0 : 1;
    in_hl = 1 - other;
    if ((place[other] != AT_REG && reg_taken(R_DE))
        || (place[0] == AT_STACK && place[1] == AT_STACK)) {
        op = compare_general(insn, op, is_signed, place, where);
        cc = op == TK_LT || op == TK_LE ? JP_C : op == TK_GE || op == TK_GT
             ? JP_NC : op == TK_EQ ? JP_Z : JP_NZ;
        branch_on(cc, branch, blk, at);

        return 1;
    }

    if (is_signed && const_number(&insn->in[other], &number)) {
        load_into(&insn->in[in_hl], place[in_hl], where[in_hl], R_HL);
        ld_rr_imm(R_DE, 0x800000);
        add_hl_rr(R_DE);
        ld_rr_imm(R_DE, (number + 0x800000) & 0xffffff);
        where[other] = R_DE;
    } else {
        carry = is_signed;
        if (place[other] == AT_STACK) {
            load_into(&insn->in[other], AT_STACK, 0, R_DE);
            place[other] = AT_REG;
            where[other] = R_DE;
        }
        load_into(&insn->in[in_hl], place[in_hl], where[in_hl], R_HL);
        if (place[other] != AT_REG) {
            load_into(&insn->in[other], place[other], where[other], R_DE);
            where[other] = R_DE;
        }
    }

    /* HL is the side in_hl names: with it on the left, `op` as it is. */
    if (in_hl == 1)
        op = mirror(op);
    if (op == TK_LE || op == TK_GT)
        out_byte(0x37);                 /* scf: HL - side - 1 */
    else
        or_a_a();
    sbc_hl_rr(where[other]);
    if (carry)
        signed_carry();
    cc = op == TK_LT || op == TK_LE ? JP_C : op == TK_GE || op == TK_GT ? JP_NC
         : op == TK_EQ ? JP_Z : JP_NZ;
    branch_on(cc, branch, blk, at);

    return 1;
}

/* Whether an add or subtract's operands are ints, or a pointer and an int
 * with a step of one byte. */
static int plain_sum(const Ins *insn)
{
    Type left = insn->in[0].attr.type, right = insn->in[1].attr.type;

    if (type_pointer(left) && type_pointer(right))
        return 0;
    if (type_pointer(left))
        return type_step(left, insn->in[0].attr.ext) == 1;
    if (type_pointer(right))
        return type_step(right, insn->in[1].attr.ext) == 1;

    return 1;
}

/* An add or a subtract: in place with inc or dec when a register's value
 * steps by a little and goes on in the same register; in HL otherwise,
 * with the other side added where it is. */
static int native_sum(const Ins *insn, int at)
{
    int op = (int) insn->rec->arg[0], place[2], where[2] = { 0, 0 };
    int res = insn->res, in_hl = 0, other = 1, number;

    (void) at;
    if ((op != TK_PLUS && op != TK_MINUS) || insn->nin != 2 || res < 0
        || insn->rec->arg[1] || type_size(vals[res].type) != ACC_INT_SIZE
        || !plain_sum(insn))
        return 0;
    place[0] = operand_place(&insn->in[0], &where[0]);
    place[1] = operand_place(&insn->in[1], &where[1]);
    if (place[0] == AT_NONE || place[1] == AT_NONE)
        return 0;

    /* x + k or x - k with x's register the answer's too, read last here. */
    if (vals[res].reg != HOME_SLOT && !vals[res].fwd && place[0] == AT_REG
        && where[0] == vals[res].reg && insn->kills & 1
        && const_number(&insn->in[1], &number) && number >= -4 && number <= 4
        && native_ready(insn)) {
        step_reg(where[0], op == TK_PLUS ? number : -number);
        pins_release(insn);
        pin_add(res);
        return 1;
    }

    if (op == TK_PLUS && place[0] == AT_REG && place[1] != AT_REG) {
        in_hl = 1;
        other = 0;
    }
    if (place[other] != AT_REG
        && !(const_number(&insn->in[other], &number) && number >= -4
             && number <= 4)
        && reg_taken(R_DE))
        return 0;                       /* DE is wanted, and holds a value */
    if (place[0] == AT_STACK && place[1] == AT_STACK)
        return 0;
    if (!native_ready(insn))
        return 0;
    if (place[other] == AT_STACK) {
        load_into(&insn->in[other], AT_STACK, 0, R_DE);
        place[other] = AT_REG;
        where[other] = R_DE;
    }
    load_into(&insn->in[in_hl], place[in_hl], where[in_hl], R_HL);
    if (const_number(&insn->in[other], &number) && number >= -4 && number <= 4) {
        step_reg(R_HL, op == TK_PLUS ? number : -number);
    } else {
        if (place[other] != AT_REG) {
            load_into(&insn->in[other], place[other], where[other], R_DE);
            where[other] = R_DE;
        }
        if (op == TK_PLUS) {
            add_hl_rr(where[other]);
        } else {
            or_a_a();
            sbc_hl_rr(where[other]);
        }
    }
    pins_release(insn);
    result_in_hl(res);

    return 1;
}

/* ++ or -- of a local that is values: inc or dec where the value is, when
 * the new one lives there too. */
static int native_step(const Ins *insn, int at)
{
    int place, where = 0, res = insn->res, val = insn->in[0].val, count, pin;
    Type type = insn->local_type;
    int byte = type == TY_UCHAR;

    (void) at;
    if (val < 0 || (type_size(type) != ACC_INT_SIZE && !byte)
        || type_float(type)
        || (type_pointer(type) && type_step(type, insn->in[0].attr.ext) != 1))
        return 0;
    count = insn->step_op == TK_MINUS ? -1 : 1;

    /* In place, where the value and the new one share a register and this
     * reads it last: nothing on the classic stack is touched, so whatever
     * waits there may. An unsigned char steps its byte alone, and wraps as
     * the type does, the bytes above staying 0. */
    pin = pin_index(val);
    if (vals[val].reg != HOME_SLOT && vals[res].reg == vals[val].reg
        && insn->kills & 1 && pin >= 0 && vstack[pin].kind == VAL_REG
        && vstack[pin].val == vals[val].reg) {
        if (byte)
            out_byte((vals[val].reg == R_BC ? 0x0c : 0x1c) + (count < 0));
        else
            step_reg(vals[val].reg, count);     /* inc/dec c or e, above */
        pins_release(insn);
        pin_add(res);
        return 1;
    }
    if (byte)
        return 0;
    place = operand_place(&insn->in[0], &where);
    if (place == AT_NONE || !native_ready(insn))
        return 0;
    if (place == AT_SLOT && iy_local && where == iy_local
        && vals[res].reg == HOME_SLOT && vals[res].slot == iy_local) {
        step_reg(-1, count);
        pins_release(insn);
        return 1;
    }
    load_into(&insn->in[0], place, where, R_HL);
    step_reg(R_HL, count);
    pins_release(insn);
    result_in_hl(res);

    return 1;
}

/* ------------------------------------------------------------------ */
/* a backend of opt-acc's own: leaf functions                          */

/* With OPTACC_LEAF, a function -- milestone 3's first slice of a backend
 * of its own, since grown to take calls and longs -- is made here from its
 * SSA form, every instruction selected here and none through gen.h. HL is
 * the accumulator, and holds the value on top of the stack the log's calls
 * kept; the values under it wait on the machine stack, pushed. DE is the
 * other side of a binary operator and A the byte of a narrow one. Values
 * that live longer are in BC or IY, or in a frame slot -- homes as
 * plan_homes gives them, BC alone of the pairs, since DE and HL are this
 * code's. A long is in a four-byte slot of its own, its operators applied
 * there (leaf_wide), or held as its low three bytes where nothing reads
 * more (leaf_long_ok); a float, by the first pass's code (leaf_delegate).
 * Anything it does not make declines the whole function, which goes to
 * the code before. */

/* ssa_leaf_off: the hybrid path's code wanted instead, because the leaf
 * backend's lost the pick -- see genlog.c. ssa_made_leaf: whether the code
 * just made was the leaf backend's. */
int ssa_leaf_off, ssa_made_leaf;

/* ssa_cached_refused: the form had cached locals but was not the leaf
 * backend's, which alone makes them: made again with ssa_leaf_off. */
int ssa_cached_refused;

/* ssa_cache_off: which locals are cached -- CACHE_NONE for the pick to
 * weigh the leaf backend's code without them, CACHE_LOOPS only those a
 * loop reads or writes; ssa_cached_used: the form just made had one. */
int ssa_cache_off, ssa_cached_used;

static int leaf_on(void)
{
    return getenv("OPTACC_LEAF") != NULL && !ssa_leaf_off;
}

static const char *leaf_why;    /* why the function is not made here */

/* The type each value made here is held as -- a byte read, widened; a
 * narrowing's -- where its own type, the first pass's, says less; and the
 * type of what is in HL now, which leaf_result gives the value made. */
static Type *leaf_holds, leaf_hl_type;

/* And how many of its bytes can be other than zero, where the code here
 * knows it better than the first pass's record does -- an AND's answer, an
 * OR of two such: 1 to 3, as vwidth says it, and the same for a value made
 * in more than one place. */
static unsigned char *leaf_widths, leaf_hl_width = 3;
static int *leaf_global_off;    /* a HOME_GLOBAL value's offset from its symbol */

/* Whether a type is one the code here holds: an int or narrower, a
 * pointer, not a short -- which it would have to widen through IY -- and
 * not a _Bool, whose steps and conversions are to 0 or 1. */
static int leaf_type(Type type)
{
    return type != TY_VOID && type != TY_BOOL && !type_is_struct(type)
           && !type_wide(type)
           && !type_float(type) && type_size(type) != 2
           && type_size(type) <= ACC_INT_SIZE;
}

static int leaf_const(const Ent *ent, int *number)
{
    if (ent->val == S_CONST && ent->attr.kind == VAL_WIDE) {
        *number = (int) (uint32_t) ent->wide;
        return 1;
    }

    return ent->val == S_CONST && ent->attr.kind == VAL_CONST
           && (*number = ent->attr.val, 1);
}

/* The width a value is known to have, 1 to 3 bytes: a constant's own, or
 * what the first pass knew of it (vwidth). */
static int leaf_width(const Ent *ent)
{
    Value attr = ent->attr;

    if (ent->val == S_CONST && attr.kind != VAL_CONST)
        return 3;

    /* An unsigned narrow value is widened with zeros in a register. */
    if (ent->val >= 0 && type_unsigned(vals[ent->val].type)
        && !type_pointer(vals[ent->val].type)
        && type_size(vals[ent->val].type) < ACC_INT_SIZE)
        return type_size(vals[ent->val].type);

    return vwidth(&attr);
}

/* Whether an operand is a constant 0 to 255: OR or XOR with one changes
 * the low byte alone, whatever the other side holds above it. */
static int leaf_byte_const(const Ent *ent)
{
    int number;

    return leaf_const(ent, &number) && number >= 0 && number <= 0xff;
}

/* Whether an operator is one the code here makes, as its operands are:
 * add and subtract, a pointer's by a step of 1 to 4; the comparisons; and
 * the rest -- AND, OR, XOR, shifts, multiply, divide, remainder -- in HL
 * where it can, and by the first pass's helpers where it cannot. */
static int leaf_operator(const Ins *insn, int arith)
{
    Type left = insn->in[0].attr.type, right = insn->in[1].attr.type;
    int step;

    switch (arith) {
    case TK_LT: case TK_GT: case TK_LE: case TK_GE: case TK_EQ: case TK_NE:
        return 1;
    case TK_PLUS: case TK_MINUS:
        if (type_pointer(left) && type_pointer(right))
            step = type_step(left, insn->in[0].attr.ext);
        else if (type_pointer(left))
            step = type_step(left, insn->in[0].attr.ext);
        else if (type_pointer(right))
            step = type_step(right, insn->in[1].attr.ext);
        else
            step = 1;
        if (step >= 1)
            return 1;                   /* past 4, scaled or divided -- see
                                         * leaf_wide_step */
        return leaf_why = "a pointer's step the code here does not make", 0;
    case TK_SHL: case TK_SHR: case TK_STAR: case TK_SLASH: case TK_PERCENT:
        return 1;                       /* in HL, or by the helper */
    case TK_AMP: case TK_PIPE: case TK_CARET:
        return 1;                       /* in A, or by the helper */
    }

    return leaf_why = "an operator the code here does not make", 0;
}

/* Whether a call is one the code here makes: to a function by name, not
 * one of those gen_call makes in place of a call -- memcpy and the rest,
 * exit -- nor setjmp or longjmp, with its parameters and its answer ints
 * or narrower. */
static int leaf_call_ok(const Ins *insn)
{
    static const char *const builtins[] = {
        "memcpy", "memmove", "memset", "exit", "setjmp", "longjmp", NULL
    };
    const Sym *callee = sym_at((int) insn->rec->arg[0]);
    int first = (int) insn->rec->arg[2], nparams = (int) insn->rec->arg[3];
    int at;

    for (at = 0; builtins[at]; at++)
        if (!strcmp(name_text(callee->name), builtins[at]))
            return leaf_why = "a call gen_call makes in place", 0;
    if (callee->type != TY_VOID && callee->type != TY_BOOL
        && !leaf_type(callee->type) && !leaf_long(callee->type))
        return leaf_why = "a call answering wider than an int", 0;
    for (at = 0; at != nparams; at++) {
        Type param = sym_param_type(first, at);

        if (!leaf_type(param) && param != TY_BOOL && !leaf_long(param))
            return leaf_why = "a call taking wider than an int", 0;
        if (leaf_long(param) && at < insn->nin
            && !leaf_long(insn->in[at].attr.type))
            return leaf_why = "a call taking a long from an int", 0;
    }

    return 1;
}

/* Whether a call's argument goes as a long, in two slots: one to a long
 * parameter, or one past the parameters that is a long. */
static int leaf_long_arg(const Ins *insn, int arg)
{
    int first = (int) insn->rec->arg[2], nparams = (int) insn->rec->arg[3];

    return arg < nparams ? leaf_long(sym_param_type(first, arg))
                         : leaf_long(insn->in[arg].attr.type);
}

/* A byte's operator -- the parser having seen that only the answer's byte
 * is kept, of the type it passes -- that the code here makes in A: + - & |
 * ^ of any operands, whose low bytes are all that matter; a shift by a
 * constant up to 8, left of anything, right only of a byte already. Each
 * side a constant, or a value the code here can have the byte of in A. */
static int leaf_narrow_ok(const Ins *insn, int op)
{
    int number, side;

    for (side = 0; side != 2; side++)
        if (!(insn->in[side].val == S_CONST && insn->in[side].attr.kind == VAL_CONST)
            && insn->in[side].val < 0)
            return 0;               /* a constant number, or a value anywhere */
    switch (op) {
    case TK_PLUS: case TK_MINUS: case TK_AMP: case TK_PIPE: case TK_CARET:
        return insn->in[0].val != S_CONST || insn->in[1].val != S_CONST;
    case TK_SHL:
        return leaf_const(&insn->in[1], &number) && number >= 0 && number <= 8
               && insn->in[0].val >= 0;
    case TK_SHR:
        return leaf_const(&insn->in[1], &number) && number >= 0 && number <= 8
               && insn->in[0].val >= 0 && type_size(vals[insn->in[0].val].type) == 1;
    }

    return 0;
}

/* Whether a type is a long: four bytes, an integer. */
static int leaf_long(Type type)
{
    return (Type) (type & ~TY_UNSIGNED) == TY_LONG;
}

/* Whether a long operator's answer has low three bytes that only its
 * operands' low three bytes make: +, -, *, &, | and ^, and a shift left
 * by a constant -- worked out as an int, then, where only those bytes of
 * its answer are kept. */
static int leaf_low_op(const Ins *insn)
{
    int count;

    if (insn->op != GL_vapply || insn->rec->arg[1] || insn->nin != 2)
        return 0;
    switch ((int) insn->rec->arg[0]) {
    case TK_PLUS: case TK_MINUS: case TK_STAR:
    case TK_AMP: case TK_PIPE: case TK_CARET:
        return 1;
    case TK_SHL:
        return leaf_const(&insn->in[1], &count) && count >= 0 && count < 24;
    }

    return 0;
}

/* By value, whether leaf_long_ok has said: -1 not yet asked. */
static signed char *leaf_low_said;

/* By value, for leaf_long_ok: its uses as a list -- each operand that
 * reads it, as the instruction it is in -- and whether a phi takes it,
 * made in one pass over the form rather than looked for over the whole of
 * it for each value. Made again where the instructions have moved since
 * (sink_steps). */

static void low_index(void)
{
    int at, operand, phi, pred, n = 0;

    luse_head = realloc(luse_head, ((size_t) nvals + 1) * sizeof *luse_head);
    lphi_in = realloc(lphi_in, (size_t) nvals + 1);
    if (!luse_head || !lphi_in)
        acc_error("out of memory for the SSA form");
    memset(lphi_in, 0, (size_t) nvals + 1);
    for (at = 0; at != nvals; at++)
        luse_head[at] = -1;
    for (at = ninsns - 1; at >= 0; at--)
        for (operand = insns[at].nin - 1; operand >= 0; operand--) {
            int val = insns[at].in[operand].val;

            if (val < 0)
                continue;
            if (n == luse_cap) {
                luse_cap = luse_cap ? luse_cap * 2 : 256;
                luse_next = realloc(luse_next, (size_t) luse_cap * sizeof *luse_next);
                luse_at = realloc(luse_at, (size_t) luse_cap * sizeof *luse_at);
                if (!luse_next || !luse_at)
                    acc_error("out of memory for the SSA form");
            }
            luse_at[n] = at;
            luse_next[n] = luse_head[val];
            luse_head[val] = n++;
        }
    for (phi = 0; phi != nphis; phi++)
        for (pred = 0; pred != preds[phis[phi].block].count; pred++)
            if (phis[phi].in[pred] >= 0)
                lphi_in[phis[phi].in[pred]] = 1;
    lindex_ok = 1;
}

/* Whether a long value is one the code here can hold as its low three
 * bytes: every use of it keeping no more than those -- a conversion or a
 * store to an int or narrower, not to a _Bool, whose truth is all four;
 * a pointer stepped by it; an operator of leaf_low_op whose own answer is
 * held so -- and no phi taking it. */
static int leaf_long_ok(int val)
{
    int e;

    if (leaf_low_said && leaf_low_said[val] >= 0)
        return leaf_low_said[val];
    if (leaf_low_said)
        leaf_low_said[val] = 0;
    if (!lindex_ok)
        low_index();
    if (lphi_in[val])
        return 0;
    for (e = luse_head[val]; e >= 0; e = luse_next[e]) {
        const Ins *use = &insns[luse_at[e]];
        Type to;

        switch (use->op) {
        case GL_vcast: case GL_vconvert:
            to = (Type) use->rec->arg[0];
            break;
        case I_CONV:
            to = use->local_type;
            break;
        case GL_vstore_local:
            to = (Type) use->rec->arg[1];
            break;
        case GL_vdrop:
            continue;
        case GL_vapply:
            if (use->res < 0 || use->rec->arg[1])
                return 0;
            if (type_pointer(vals[use->res].type)
                && ((int) use->rec->arg[0] == TK_PLUS
                    || (int) use->rec->arg[0] == TK_MINUS))
                continue;                   /* a pointer's step */
            if (leaf_low_op(use) && leaf_long(vals[use->res].type)
                && leaf_long_ok(use->res))
                continue;
            return 0;
        default:
            return 0;
        }
        if (leaf_long(to) && use->op != GL_vstore_local && use->res >= 0
            && leaf_long_ok(use->res))
            continue;                       /* a long again, held so */
        if (!leaf_type(to) || to == TY_BOOL)
            return 0;
    }
    if (leaf_low_said)
        leaf_low_said[val] = 1;

    return 1;
}

/* Whether a value of `type` is one the code here holds: a scalar no
 * wider than an int, a _Bool, or a struct or an array -- its address. */
static int leaf_value_type(Type type)
{
    return leaf_type(type) || type == TY_BOOL || type_is_struct(type)
           || type_is_array(type);
}

/* How far `x++` moves a `type` reached through a pointer: 1 for a
 * number, the size of what it points at for a pointer -- a scalar's,
 * whose size needs no more than its type -- and 0 where that is not 1 to
 * 4, which the code here does not make with the address in DE. */
static int leaf_indirect_step(Type type)
{
    Type to;

    if (!type_pointer(type))
        return 1;
    to = type_deref(type);
    if (type_is_struct(to) || (!leaf_type(to) && to != TY_BOOL))
        return 0;

    return type_size(to) >= 1 && type_size(to) <= 4 ? type_size(to) : 0;
}

/* Whether every instruction is one the code here makes. */
/* Whether an instruction with a wide value -- a long or a float -- in it
 * can be left to the first pass's code (leaf_delegate): its operands put
 * on the classic stack, the call made again, and its answer taken back.
 * The wide values themselves live in frame slots. */
static int leaf_delegable(const Ins *insn)
{
    int operand;

    switch (insn->op) {
    case GL_vapply: case GL_vconvert: case GL_vcast: case GL_vtruth:
    case GL_vneg: case GL_vnot: case GL_vpush_local: case GL_vstore_local:
    case GL_vderef: case GL_vstore_indirect: case I_CONV: case I_STEP:
    case GL_vprefix_indirect: case GL_vpostfix_indirect:
    case GL_vprefix_local: case GL_vpostfix_local: case GL_gen_return:
        break;
    case GL_vdrop:
        return 1;                       /* a value in a slot: nothing */
    default:
        return 0;
    }
    if (insn->rec->top.bits)
        return 0;
    for (operand = 0; operand != insn->nin; operand++)
        if (insn->in[operand].attr.bits || type_is_struct(insn->in[operand].attr.type))
            return 0;

    return 1;
}

/* ------------------------------------------------------------------ */
/* Longs made here: each value in its four-byte slot, never on the stack
 * nor in a register, and an operator the runtime's routine applied where
 * the answer is to be -- the left operand copied there, unless it is there
 * already, and the right read where it lies or from the pool. */

/* Whether a value is a long the code here holds whole, in its slot: not
 * one held as its low three bytes (leaf_long_ok). */
static int leaf_wide_val(int val)
{
    return val >= 0 && !vals[val].fwd && vals[val].reg == HOME_SLOT
           && leaf_long(vals[val].type) && !leaf_long_ok(val);
}

/* A long operand: a value whole in its slot, or a constant. */
static int leaf_wide_operand(const Ent *ent)
{
    if (ent->val == S_CONST)
        return ent->attr.kind == VAL_WIDE || ent->attr.kind == VAL_CONST;

    return leaf_wide_val(ent->val);
}

/* A constant's four bytes as a long: a narrow one widened by its own type
 * -- an unsigned int's 0xffffff is held as -1, and is not 0xffffffff. */
static uint32_t leaf_wide_bits(const Ent *ent)
{
    return ent->attr.kind == VAL_WIDE ? (uint32_t) ent->wide
                                      : (uint32_t) const_as(&ent->attr, TY_LONG);
}

/* The routine for a long operator, or -1 for one not made here. */
static int leaf_wide_helper(int arith, Type type)
{
    switch (arith) {
    case TK_SLASH:   return type_unsigned(type) ? RT_LDIVU : RT_LDIVS;
    case TK_PERCENT: return type_unsigned(type) ? RT_LREMU : RT_LREMS;
    case TK_PLUS:  return RT_LADD;
    case TK_MINUS: return RT_LSUB;
    case TK_AMP:   return RT_LAND;
    case TK_PIPE:  return RT_LOR;
    case TK_CARET: return RT_LXOR;
    case TK_STAR:  return RT_LMUL;
    case TK_SHL:   return RT_LSHL;
    case TK_SHR:   return type_unsigned(type) ? RT_LSHRU : RT_LSHRS;
    }

    return -1;
}

/* Whether an instruction is one with a long in it that the code here
 * makes itself, rather than leaving to the first pass's (leaf_delegate). */
static int leaf_wide_ok(const Ins *insn)
{
    Type from, to;

    if (insn->rec && insn->rec->top.bits)
        return 0;
    switch (insn->op) {
    case GL_vapply:
        return !insn->rec->arg[1] && insn->nin == 2 && insn->res >= 0
               && leaf_wide_val(insn->res)
               && leaf_wide_helper((int) insn->rec->arg[0], vals[insn->res].type) >= 0
               && leaf_wide_operand(&insn->in[0])
               && leaf_wide_operand(&insn->in[1]);
    case I_CONV: case GL_vconvert: case GL_vcast:
        if (insn->res < 0 || insn->nin != 1 || insn->in[0].attr.bits)
            return 0;
        from = insn->in[0].attr.type;
        to = insn->op == I_CONV ? insn->local_type : (Type) insn->rec->arg[0];
        if (leaf_long(to) && leaf_wide_val(insn->res))
            return leaf_long(from) ? leaf_wide_operand(&insn->in[0])
                                   : leaf_type(from);
        return leaf_long(from) && leaf_type(to) && to != TY_BOOL
               && leaf_wide_operand(&insn->in[0]);
    case GL_vderef:
        return insn->res >= 0 && leaf_wide_val(insn->res) && !insn->in[0].attr.bits
               && type_pointer(insn->in[0].attr.type)
               && leaf_long(type_deref(insn->in[0].attr.type))
               && (insn->in[0].val < 0 || leaf_type(vals[insn->in[0].val].type));
    case I_BR:
        return insn->nin == 1 && insn->target >= 0 && insn->in[0].val >= 0
               && leaf_wide_val(insn->in[0].val);
    case I_SET:
        return insn->nin == 1 && leaf_wide_val(insn->target)
               && leaf_wide_operand(&insn->in[0]);
    }

    return 0;
}

/* Whether an instruction makes a long held as its low three bytes from
 * operands read for theirs: an operator of leaf_low_op, or a conversion
 * from an int or narrower or from another long -- nothing made, then. */
static int leaf_low_held(const Ins *insn)
{
    Type from;

    if (insn->res < 0 || !leaf_long(vals[insn->res].type)
        || !leaf_long_ok(insn->res))
        return 0;
    if (insn->op == GL_vapply)
        return leaf_low_op(insn);
    if (insn->op != I_CONV && insn->op != GL_vconvert && insn->op != GL_vcast)
        return 0;
    from = insn->in[0].attr.type;

    return !insn->in[0].attr.bits
           && ((leaf_type(from) && from != TY_BOOL) || leaf_long(from));
}

static int leaf_ok(void)
{
    static signed char *said;
    int at, operand;
    Type returns = sym_at(gl_fn)->type;

    said = realloc(said, (size_t) nvals + 1);
    if (!said)
        acc_error("out of memory for the SSA form");
    memset(said, -1, (size_t) nvals + 1);
    leaf_low_said = said;

    /* A _Bool answer is fine: gen_return makes the 0 or 1 of it; a long
     * or a float is returned by the first pass's code, from its slot. */
    if (returns != TY_VOID && returns != TY_BOOL && !leaf_type(returns)
        && !type_wide(returns) && !type_float(returns))
        return leaf_why = "an answer wider than an int", 0;

    for (at = 1; at != ninsns; at++) {
        const Ins *insn = &insns[at];
        int op = insn->op, arith;

        /* A _Bool is a byte of 0 or 1, made so by leaf_convert; a struct
         * or an array as a value is its address; a long, only where all
         * that is ever kept of it is its low three bytes -- see
         * leaf_long_ok -- or a constant one dropped. */
        insns[at].delegated = 0;
        insns[at].wide = (unsigned char) leaf_wide_ok(insn);
        if (insn->wide)
            continue;
        for (operand = 0; operand != insn->nin; operand++)
            if (!leaf_value_type(insn->in[operand].attr.type)
                && !(leaf_long(insn->in[operand].attr.type)
                     && (insn->in[operand].val >= 0
                         ? leaf_long_ok(insn->in[operand].val)
                           || ((leaf_low_held(insn) || op == GL_gen_call)
                               && leaf_wide_val(insn->in[operand].val))
                         : insn->in[operand].val == S_CONST
                           && (op == GL_vdrop || leaf_low_held(insn)
                               || op == GL_gen_call
                               || (op == GL_vstore_indirect && operand == 1))))) {
                if (!leaf_delegable(insn))
                    return leaf_why = "an operand wider than an int", 0;
                insns[at].delegated = 1;
            }
        if (insn->res >= 0 && !leaf_value_type(vals[insn->res].type)
            && !(leaf_long(vals[insn->res].type) && leaf_long_ok(insn->res)
                 && (op == GL_vpush_local || op == GL_vderef
                     || op == GL_vstore_local || op == GL_vstore_indirect
                     || leaf_low_held(insn)))
            && !(op == GL_gen_call && leaf_long(vals[insn->res].type))) {
            if (!leaf_delegable(insn))
                return leaf_why = "a value wider than an int", 0;
            insns[at].delegated = 1;
        }
        /* A read or write through a pointer of something wide: the
         * pointer and an int written are narrow, the memory is not -- but
         * for a long read as its low three bytes, or a constant written,
         * which the code here makes. */
        if ((op == GL_vderef || op == GL_vstore_indirect)
            && type_pointer(insn->in[0].attr.type)
            && (type_wide(type_deref(insn->in[0].attr.type))
                || type_float(type_deref(insn->in[0].attr.type)))
            && !(leaf_long(type_deref(insn->in[0].attr.type))
                 && (op == GL_vderef
                     ? insn->res >= 0 && leaf_long_ok(insn->res)
                     : insn->in[1].val == S_CONST))
            && leaf_delegable(insn))
            insns[at].delegated = 1;
        if (insn->delegated)
            continue;
        switch (op) {
        case I_FRAME:
            /* The frame is laid down again as the first pass laid it, arrays
             * and all; a claim of IY only for a local that is values now,
             * or where it claims nothing -- not a `register` local. */
            switch (insn->rec->op) {
            case GL_gen_local: case GL_gen_local_array:
            case GL_gen_local_array_size: case GL_gen_local_far:
            case GL_gen_local_scope:
                continue;
            case GL_gen_iy_claim:
                if (!((int) insn->rec->arg[2] & SQ_REGISTER))
                    continue;
                break;
            }
            if (!frame_of_value(insn))
                return leaf_why = "a frame not of scalars", 0;
            continue;
        case GL_vaddr_array: case GL_vaddr_local:
            continue;
        case GL_gen_switch_load:
            /* A switch on an int or narrower: its value from its slot into
             * HL, or a char into A, and each case a comparison and a jump,
             * as the first pass makes them. */
            if (type_wide((Type) insn->rec->arg[1]))
                return leaf_why = "a switch on a long", 0;
            continue;
        case GL_gen_switch_case:
            if (type_wide((Type) insn->rec->arg[2]))
                return leaf_why = "a switch on a long", 0;
            continue;
        case GL_vprefix_local: case GL_vpostfix_local:
            /* ++ and -- of a local in memory, in its slot. */
            if (insn->rec->top.bits || !leaf_type((Type) insn->rec->arg[1])) {
                if (!leaf_delegable(insn))
                    return leaf_why = "a step of a local in memory not a scalar", 0;
                insns[at].delegated = 1;        /* the first pass's code */
            }
            continue;
        case GL_vprefix_indirect: case GL_vpostfix_indirect: {
            /* ++ and -- through a pointer. */
            Type target = type_pointer(insn->in[0].attr.type)
                          ? type_deref(insn->in[0].attr.type) : TY_VOID;

            if (insn->rec->top.bits || insn->in[0].attr.bits
                || !leaf_type(target) || !leaf_indirect_step(target)) {
                if (!leaf_delegable(insn))
                    return leaf_why = "a step through a pointer not of a scalar", 0;
                insns[at].delegated = 1;        /* the first pass's code */
                continue;
            }
            continue;
        }
        case GL_vpush_local: case GL_vstore_local:
            /* A local that stays in memory -- its address taken -- read or
             * written in its slot; a long one read for its low bytes, or
             * written from an int. */
            if (insn->rec->top.bits
                || (!leaf_type((Type) insn->rec->arg[1])
                    && (Type) insn->rec->arg[1] != TY_BOOL
                    && !(leaf_long((Type) insn->rec->arg[1])
                         && (op == GL_vpush_local
                             || leaf_type(insn->in[0].attr.type)))))
                return leaf_why = "a local in memory not a scalar", 0;
            continue;
        case I_STEP:
            if (insn->local_type == TY_BOOL)
                return leaf_why = "a _Bool stepped", 0;
            continue;
        case I_BR: case I_JMP: case I_SET: case I_CONV:
        case GL_vdrop: case GL_gen_stmt_end: case GL_gen_value_end:
        case GL_gen_return:
        case GL_vneg: case GL_vnot: case GL_vtruth: case GL_vconvert:
        case GL_vcast: case GL_vpush_bss: case GL_vpush_const:
        case GL_vpush_global_addr:
            continue;
        case GL_gen_call:
            if (!leaf_call_ok(insn))
                return 0;
            continue;
        case GL_vmember:
            if (insn->rec->top.bits || insn->res < 0
                || !type_pointer(vals[insn->res].type))
                return leaf_why = "a member not reached by its address", 0;
            continue;
        case GL_vderef: case GL_vstore_indirect:
            /* What is read or written is what the pointer points at, which
             * a short is not in the code here; nor a bit-field. A _Bool is
             * a byte, written as 0 or 1; a struct read is its address, and
             * written, a copy; an array read is its address too. */
            if (!type_pointer(insn->in[0].attr.type)
                || (!leaf_type(type_deref(insn->in[0].attr.type))
                    && type_deref(insn->in[0].attr.type) != TY_BOOL
                    && !(op == GL_vderef && leaf_long(type_deref(insn->in[0].attr.type)))
                    && !(op == GL_vstore_indirect
                         && leaf_long(type_deref(insn->in[0].attr.type))
                         && insn->in[1].val == S_CONST)
                    && !type_is_struct(type_deref(insn->in[0].attr.type))
                    && !(op == GL_vderef
                         && type_is_array(type_deref(insn->in[0].attr.type))))
                || insn->rec->top.bits || insn->in[0].attr.bits) {
                /* A short, say: the first pass's code reads or writes it. */
                if (type_pointer(insn->in[0].attr.type) && leaf_delegable(insn)) {
                    insns[at].delegated = 1;
                    continue;
                }
                return leaf_why = "a read or write not of a scalar", 0;
            }
            continue;
        case GL_gen_data:
            continue;                   /* a string's bytes, jumped over */
        case GL_vapply:
            arith = (int) insn->rec->arg[0];
            /* Narrowed to a short: the first pass's code makes it. */
            if (insn->rec->arg[1] && type_size((Type) insn->rec->arg[1]) != 1
                && leaf_delegable(insn)) {
                insns[at].delegated = 1;
                continue;
            }
            if (insn->rec->arg[1] && !leaf_narrow_ok(insn, arith))
                return leaf_why = "an operator on bytes", 0;
            if (insn->rec->arg[1])
                continue;
            if (!leaf_operator(insn, arith))
                return 0;
            continue;
        }
        return leaf_why = gl_names[op < GL_COUNT ? op : 0] ? gl_names[op < GL_COUNT ? op : 0]
                                                           : "an instruction", 0;
    }

    return 1;
}

/* The values left on the stack for the next instruction: how many, and
 * whether the top is in HL still. The rest are on the machine stack. */
static int leaf_depth, leaf_top_in_hl;

/* Or the top is a byte in A alone, not yet widened into HL: a byte read,
 * or a byte's AND, OR or XOR, left there for what takes a byte -- another
 * such operator, a byte's store, a test -- and widened, as its type says,
 * by whatever wants HL. leaf_top_in_hl is set with it. */
static int  leaf_top_in_a;
static Type leaf_a_type;

/* Instructions made with one before them, and so left out when their
 * turn comes: this one and those before it. */
static int  leaf_skip_until = -1;

static int  leaf_byte_to_a(const Ent *ent);
static int  leaf_byte_ok(const Ent *ent);
static int  leaf_mul_ok(int by);
static void leaf_mul(int by);

enum { R_AF_BYTE = 1, R_L_BYTE, R_E_BYTE };     /* where leaf_pop left a byte */

/* Where A was last stored to a byte's slot, and which: a read of that slot
 * straight after it, with nothing jumping in between, is A as it is. */
static int      leaf_a_stored_at = -1, leaf_a_stored_slot, leaf_a_store_from;
static unsigned leaf_a_stored_epoch;

/* The values under the top, on the machine stack: whether each is a byte
 * pushed with push af -- its byte then the high one of the pair it is
 * popped into -- and the byte's type. */
static unsigned char *leaf_stacked_af;
static Type          *leaf_stacked_type;
static int            leaf_nstacked, leaf_stacked_cap;

/* The byte in A widened into HL, where the top is one. */
static void leaf_widen_a(void)
{
    if (!leaf_top_in_a)
        return;
    leaf_top_in_a = 0;
    if (type_unsigned(leaf_a_type)) {
        or_a_a();
        sbc_hl_hl();
    } else {
        ld_l_a();
        out_byte2(0xcb, 0x05);          /* rlc l: the sign into carry */
        sbc_hl_hl();
    }
    ld_l_a();
}

/* The top, in HL or a byte in A, onto the machine stack. */
static void leaf_push_top(void)
{
    if (leaf_nstacked == leaf_stacked_cap) {
        leaf_stacked_cap = leaf_stacked_cap ? leaf_stacked_cap * 2 : 16;
        leaf_stacked_af = realloc(leaf_stacked_af, (size_t) leaf_stacked_cap);
        leaf_stacked_type = realloc(leaf_stacked_type,
                                    (size_t) leaf_stacked_cap * sizeof *leaf_stacked_type);
        if (!leaf_stacked_af || !leaf_stacked_type)
            acc_error("out of memory for the SSA form");
    }
    leaf_stacked_af[leaf_nstacked] = (unsigned char) leaf_top_in_a;
    leaf_stacked_type[leaf_nstacked] = leaf_a_type;
    leaf_nstacked++;
    if (leaf_top_in_a)
        out_byte(0xf5);                 /* push af */
    else
        push_rr(R_HL);
    leaf_top_in_hl = leaf_top_in_a = 0;
}

/* The value on top of the machine stack popped into `reg`, HL or DE, and
 * widened there -- or, `byte` set, left a byte: then the answer is the
 * register it is in, E, D or A. */
static int leaf_pop(int reg, int byte)
{
    int af;

    leaf_nstacked--;
    af = leaf_stacked_af[leaf_nstacked];
    if (byte && af) {
        out_byte(0xf1);                 /* pop af: the byte where it was */
        return R_AF_BYTE;
    }
    pop_rr(reg);
    if (byte)
        return reg == R_HL ? R_L_BYTE : R_E_BYTE;
    if (af) {                           /* the byte in H or D, widened */
        out_byte(reg == R_HL ? 0x7c : 0x7a);    /* ld a, h / ld a, d */
        if (reg == R_DE)
            ex_de_hl();
        leaf_top_in_a = 1;
        leaf_a_type = leaf_stacked_type[leaf_nstacked];
        leaf_widen_a();
        if (reg == R_DE)
            ex_de_hl();
    }

    return 0;
}

/* HL free for the instruction at `at`: a value left there that this one
 * does not read goes to the machine stack. */
static void leaf_free_hl(const Ins *insn)
{
    int operand, reads_top = 0;

    for (operand = 0; operand != insn->nin; operand++)
        if (insn->in[operand].val >= 0 && vals[insn->in[operand].val].fwd)
            reads_top = 1;
    if (leaf_top_in_hl && !reads_top)
        leaf_push_top();
}

/* An operand, not one left on the stack, into `reg`, HL or DE. */
static void leaf_load(const Ent *ent, int reg)
{
    int val = ent->val;
    Type type;

    if (val == S_CONST || val == UNDEF) {
        int number = val == UNDEF ? 0 : ent->attr.kind == VAL_WIDE
                     ? (int) (uint32_t) ent->wide : ent->attr.val;

        if (val != UNDEF && ent->attr.kind == VAL_ADDR)
            out_reloc(out_here() + 1);
        else if (val != UNDEF && ent->attr.kind == VAL_BSS)
            gen_bss_fixup(out_here() + 1);
        ld_rr_imm(reg, val == UNDEF ? 0 : ent->attr.kind == VAL_ADDR
                               ? moved_at(number) : number);
        return;
    }
    type = vals[val].type;
    if (vals[val].reg == HOME_GLOBAL) {
        ld_rr_imm(reg, leaf_global_off[val]);   /* the link adds the address */
        fixup_add(vals[val].slot, out_here() - ACC_INT_SIZE);
    } else if (vals[val].reg == R_BC) {
        push_rr(R_BC);
        pop_rr(reg);
    } else if (vals[val].reg == HOME_SLOT && iy_web >= 0
               && vals[val].slot == fixed_slot[iy_web]) {
        lea_rr_iy(reg, 0);
    } else if (type_size(type) >= ACC_INT_SIZE) {
        ld_rr_ix(reg, vals[val].slot);          /* a long's low three bytes */
    } else {
        load_narrow_into(reg, vals[val].slot, type);
    }
}

/* The top of the stack into HL, where it is not. */
static void leaf_top_to_hl(void)
{
    leaf_widen_a();
    if (!leaf_top_in_hl) {
        (void) leaf_pop(R_HL, 0);
        leaf_top_in_hl = 1;
    }
}

/* One operand into HL -- the stack's top, or loaded. */
static void leaf_operand_hl(const Ins *insn, int operand)
{
    const Ent *ent = &insn->in[operand];

    if (ent->val >= 0 && vals[ent->val].fwd) {
        leaf_top_to_hl();
        leaf_depth--;
        leaf_top_in_hl = 0;
    } else {
        leaf_load(ent, R_HL);
    }
}

static int leaf_read_once(int val);

/* Whether `ent` is a byte in a frame slot that A was stored to just now,
 * nothing made since: A holds it. */
static int leaf_a_has(const Ent *ent)
{
    int val = ent->val;

    return val >= 0 && !vals[val].fwd && vals[val].reg == HOME_SLOT
           && !(iy_web >= 0 && vals[val].slot == fixed_slot[iy_web])
           && type_size(vals[val].type) == 1
           && leaf_a_stored_at == out_here() && leaf_a_stored_slot == vals[val].slot
           && leaf_a_stored_epoch == out_rewinds && join_at != out_here();
}

/* A binary operator's two operands: the left into HL, the right into DE
 * -- or, with `bc_too`, left in BC where it lives, for an instruction with
 * a form for BC. Answers the right's register. */
static int leaf_operands_in(const Ins *insn, int bc_too)
{
    const Ent *left = &insn->in[0], *right = &insn->in[1];
    int lfwd = left->val >= 0 && vals[left->val].fwd;
    int rfwd = right->val >= 0 && vals[right->val].fwd;

    if (lfwd && rfwd) {
        leaf_top_to_hl();
        ex_de_hl();                     /* the right, on top */
        (void) leaf_pop(R_HL, 0);       /* the left, under it */
        leaf_depth -= 2;
    } else if (rfwd) {
        leaf_top_to_hl();
        ex_de_hl();
        leaf_depth--;
        leaf_load(left, R_HL);
    } else if (lfwd) {
        leaf_top_to_hl();
        leaf_depth--;
        if (bc_too && right->val >= 0 && vals[right->val].reg == R_BC) {
            leaf_top_in_hl = 0;
            return R_BC;
        }
        leaf_load(right, R_DE);
    } else if (leaf_a_has(right) && type_unsigned(vals[right->val].type)) {
        /* An unsigned byte stored from A just now: into DE from A, before
         * the left comes to HL -- and not stored at all where this is its
         * one read. */
        if (leaf_read_once(right->val))
            out_rewind(leaf_a_store_from);
        leaf_a_stored_at = -1;
        ld_rr_imm(R_DE, 0);
        out_byte(0x5f);                 /* ld e, a */
        leaf_load(left, R_HL);
    } else {
        leaf_load(left, R_HL);
        if (bc_too && right->val >= 0 && vals[right->val].reg == R_BC)
            return R_BC;
        leaf_load(right, R_DE);
    }
    leaf_top_in_hl = 0;

    return R_DE;
}


/* The value `val`, made in HL, to where it lives -- or left on the stack. */
static void leaf_result(int val)
{
    /* Held as what made it -- but not a value made in more than one
     * place, the answer of ?: or a phi, which each makes differently. */
    if (val >= 0 && leaf_holds) {
        leaf_holds[val] = def_at[val] >= 0 ? leaf_hl_type : TY_VOID;
        leaf_widths[val] = def_at[val] >= 0 ? leaf_hl_width : 3;
    }
    leaf_hl_type = TY_VOID;
    leaf_hl_width = 3;
    if (val < 0 || !vals[val].used)
        return;
    if (vals[val].fwd) {
        leaf_depth++;
        leaf_top_in_hl = 1;
        return;
    }
    if (vals[val].reg == R_BC) {
        push_rr(R_HL);
        pop_rr(R_BC);
    } else if (iy_web >= 0 && vals[val].slot == fixed_slot[iy_web]) {
        push_rr(R_HL);
        out_byte2(0xfd, 0xe1);          /* pop iy */
    } else if (type_size(vals[val].type) >= ACC_INT_SIZE) {
        ld_ix_rr(vals[val].slot, R_HL);         /* a long's low three bytes */
    } else {
        store_narrow(vals[val].slot, vals[val].type);
    }
}

/* The value `val`, made in A: a byte, `type`. The stack's top stays in A
 * for what takes a byte; a byte's slot takes it from A; anywhere else it
 * is widened into HL first. */
static void leaf_result_a(int val, Type type)
{
    int to_slot = val >= 0 && vals[val].used && !vals[val].fwd
                  && vals[val].reg == HOME_SLOT
                  && !(iy_web >= 0 && vals[val].slot == fixed_slot[iy_web])
                  && type_size(vals[val].type) == 1;

    leaf_hl_type = type;
    if (leaf_hl_width == 3 && type_unsigned(type))
        leaf_hl_width = 1;
    if (val < 0 || !vals[val].used) {
        leaf_result(val);
        return;
    }
    if (to_slot) {
        leaf_a_store_from = out_here();
        ld_ix_a(vals[val].slot);
        leaf_a_stored_at = out_here();
        leaf_a_stored_slot = vals[val].slot;
        leaf_a_stored_epoch = out_rewinds;
        vals[val].used = 0;             /* stored: leaf_result notes it only */
        leaf_result(val);
        vals[val].used = 1;
        return;
    }
    if (val >= 0 && vals[val].used && vals[val].fwd) {
        leaf_result(val);
        leaf_top_in_a = 1;
        leaf_a_type = type;
        return;
    }
    leaf_top_in_a = 1;                  /* widened as it would be on top */
    leaf_a_type = type;
    leaf_widen_a();
    leaf_result(val);
}

/* Whether a value held as `from` is in `to`'s range already, so that
 * narrowing it to `to` would change nothing: the same type, or an
 * unsigned one narrower, or a signed one no wider going to signed. A
 * value held as _Bool is 0 or 1, which every type holds. */
static int leaf_fits(Type from, Type to)
{
    if (from == to || from == TY_BOOL || type_size(to) == ACC_INT_SIZE)
        return 1;
    if (type_size(from) < type_size(to) && type_unsigned(from)
        && !type_pointer(from))
        return 1;

    return type_size(from) <= type_size(to) && !type_unsigned(from)
           && !type_unsigned(to);
}

/* The type an operand is held as in a register: what it was made as, or a
 * constant's range. */
static Type leaf_held(const Ent *ent)
{
    int number;

    if (leaf_const(ent, &number))
        return number >= 0 && number <= 0xff ? TY_UCHAR
               : number >= -128 && number <= 127 ? TY_CHAR : TY_INT;
    if (ent->val >= 0 && leaf_holds && leaf_holds[ent->val] != TY_VOID)
        return leaf_holds[ent->val];

    return ent->val >= 0 ? vals[ent->val].type : ent->attr.type;
}

/* HL made the narrow type `type` from what is in L: its byte widened --
 * or from what is in HL's low two bytes, for a short: the top byte cleared,
 * or filled with bit 15, through DE. */
static void leaf_narrow(Type type)
{
    if (type_size(type) == 2) {
        leaf_hl_type = type;
        ex_de_hl();
        if (type_unsigned(type)) {
            ld_rr_imm(R_HL, 0);
            leaf_hl_width = 2;
        } else {
            out_byte(0x7a);                 /* ld a, d */
            out_byte(0x17);                 /* rla: bit 15 into carry */
            sbc_hl_hl();
        }
        out_byte2(0x62, 0x6b);              /* ld h, d; ld l, e */
        return;
    }
    if (type_size(type) != 1)
        return;
    leaf_hl_type = type;
    ld_a_l();
    if (type_unsigned(type)) {
        or_a_a();
        sbc_hl_hl();
    } else {
        out_byte2(0xcb, 0x05);          /* rlc l: the sign into carry */
        sbc_hl_hl();
    }
    ld_l_a();
}

/* A byte read from (HL) -- or from `(iy+d)` with `iy` -- into HL, widened
 * as `type` wants; or the int there. */
static void leaf_read(Type type, int iy, int disp)
{
    leaf_hl_type = type;
    if (type_size(type) == ACC_INT_SIZE) {
        if (iy)
            out_byte3(0xfd, 0x27, disp);        /* ld hl, (iy+d) */
        else
            ld_hl_ind_hl();
        return;
    }
    if (iy)
        out_byte3(0xfd, 0x7e, disp);            /* ld a, (iy+d) */
    else
        ld_a_hl();
    if (type_unsigned(type)) {
        or_a_a();
        sbc_hl_hl();
    } else {
        ld_l_a();
        out_byte2(0xcb, 0x05);                  /* rlc l */
        sbc_hl_hl();
    }
    ld_l_a();
}

/* Whether the value is the one in IY. */
static int leaf_in_iy(const Ent *ent)
{
    return ent->val >= 0 && !vals[ent->val].fwd && vals[ent->val].reg == HOME_SLOT
           && iy_web >= 0 && vals[ent->val].slot == fixed_slot[iy_web];
}

/* The branch that the truth value `val`, made at `at`, reaches through
 * nothing but conversions and truth tests of it -- `(x & 4) != 0` made a
 * _Bool, and `!` of that -- or -1. `*cc` is turned about by each `!`: a
 * conversion of 0 or 1 to any type the code here makes is 0 or 1 still. */
static int leaf_truth_branch(int at, int val, int *cc)
{
    int next;

    for (next = at + 1; next < ninsns && val >= 0 && vals[val].fwd; next++) {
        const Ins *insn = &insns[next];

        if (insn->op == GL_vdrop && insn->nin == 0)
            continue;
        if (insn->nin != 1 || insn->in[0].val != val)
            return -1;
        if (insn->op == I_BR)
            return insn->target >= 0 ? next : -1;
        if (insn->op == GL_vtruth && (int) insn->rec->arg[0] == TK_EQ)
            *cc ^= 0x08;
        else if (insn->op != GL_vtruth && insn->op != GL_vconvert)
            return -1;
        val = insn->res;
    }

    return -1;
}

/* The jump for the branch that the truth value made at `at` reaches, on
 * flags `cc` true, made with it and the steps between left out; 0 if
 * there is none, and the value is to be made. */
static int leaf_branch(int cc, int blk, int at)
{
    int next = leaf_truth_branch(at, insns[at].res, &cc);

    if (next < 0)
        return 0;
    branch_to(cc, &insns[next], blk);
    skip_branch = next;
    fused_from = at;

    return 1;
}

/* An AND with a byte, made in A, and compared with 0 by what follows: the
 * branch that comparison reaches taken on the AND's own flags, and the
 * comparison and what is between left out. */
static int leaf_and_branch(const Ins *insn, int blk, int at)
{
    int next = at + 1, op, zero;
    const Ins *test;

    while (next < ninsns && insns[next].op == GL_vdrop && insns[next].nin == 0)
        next++;
    if (next == ninsns || insn->res < 0 || !vals[insn->res].fwd)
        return 0;
    test = &insns[next];
    op = test->op == GL_vapply ? (int) test->rec->arg[0] : 0;
    if ((op != TK_EQ && op != TK_NE) || test->nin != 2
        || test->in[0].val != insn->res || !leaf_const(&test->in[1], &zero)
        || zero != 0 || !leaf_branch(op == TK_EQ ? JP_Z : JP_NZ, blk, next))
        return 0;
    fused_from = at;

    return 1;
}

/* A constant's bytes applied to the value in HL where the top byte needs
 * nothing done to it, which has no name: an AND that clears it -- the
 * middle byte first, then sbc hl, hl on the carry it clears, the low byte
 * waiting in E -- or any operator whose constant leaves it as it is. */
static int leaf_const_bytes(int op, int value, int emit)
{
    int low = value & 0xff, middle = (value >> 8) & 0xff, top = (value >> 16) & 0xff;
    int identity = op == TK_AMP ? 0xff : 0x00;
    int imm = op == TK_AMP ? 0xe6 : op == TK_PIPE ? 0xf6 : 0xee;

    if (op == TK_AMP && top == 0x00) {
        if (!emit)
            return 1;
        if (low) {
            ld_a_l();
            if (low != 0xff)
                out_byte2(0xe6, low);           /* and n */
            out_byte(0x5f);                     /* ld e, a */
        }
        ld_a_h();
        if (middle == 0xff)
            or_a_a();                           /* the carry, cleared */
        else
            out_byte2(0xe6, middle);            /* and n, which clears it */
        sbc_hl_hl();
        ld_h_a();
        if (low)
            out_byte(0x6b);                     /* ld l, e */
        return 1;
    }
    if (top != identity)
        return 0;
    if (!emit)
        return 1;
    if (low != identity) {
        ld_a_l();
        out_byte2(imm, low);
        ld_l_a();
    }
    if (middle != identity) {
        ld_a_h();
        out_byte2(imm, middle);
        ld_h_a();
    }

    return 1;
}

/* How many bytes of an operand can be other than zero, as it is made:
 * what the first pass knew, or what the code here knew when it made it. */
static int leaf_known_width(const Ent *ent)
{
    int width = leaf_width(ent);

    if (ent->val >= 0 && leaf_widths[ent->val] < width)
        width = leaf_widths[ent->val];

    return width;
}

/* Whether an operand is a global's address the link fills in, loaded where
 * it is read: then a read or a write through it is (nn). */
static int leaf_global(const Ent *ent)
{
    if (ent->val == S_CONST)            /* or a constant one: bss, image */
        return ent->attr.kind == VAL_CONST || ent->attr.kind == VAL_BSS
               || ent->attr.kind == VAL_ADDR;

    return ent->val >= 0 && !vals[ent->val].fwd && vals[ent->val].reg == HOME_GLOBAL;
}

/* That address, as the operand of the instruction just begun: the offset,
 * and the fixup the link adds the address to -- or the constant, with what
 * moves it, as leaf_load makes one. */
static void leaf_global_nn(const Ent *ent)
{
    if (ent->val == S_CONST) {
        if (ent->attr.kind == VAL_ADDR)
            out_reloc(out_here());
        else if (ent->attr.kind == VAL_BSS)
            gen_bss_fixup(out_here());
        out_word24(ent->attr.kind == VAL_ADDR ? moved_at(ent->attr.val) : ent->attr.val);
        return;
    }
    out_word24(leaf_global_off[ent->val]);
    fixup_add(vals[ent->val].slot, out_here() - ACC_INT_SIZE);
}

/* A with the byte of `ent`, not on the stack, by the operator whose
 * A-with-(ix+d) form is `with_ix`: a constant's byte, a slot's in place, and
 * anything else through DE -- which none of those loads by way of A. */
static void leaf_alu_byte(int with_ix, const Ent *ent)
{
    int number;

    if (leaf_const(ent, &number)) {
        out_byte2(with_ix + 0x40, number & 0xff);       /* op n */
    } else if (vals[ent->val].reg == HOME_SLOT
               && !(iy_web >= 0 && vals[ent->val].slot == fixed_slot[iy_web])) {
        frame_byte(with_ix, vals[ent->val].slot);        /* op (ix+d) */
    } else if (vals[ent->val].reg == R_BC) {
        out_byte(with_ix - 5);                           /* op c */
    } else {
        leaf_load(ent, R_DE);
        out_byte(with_ix - 3);                           /* op e */
    }
}

/* A byte's operator, made in A: the answer's byte is all that is kept, as
 * the type `narrow` -- see leaf_narrow_ok. The side on top of the stack
 * into A, the other from where it is; a subtraction with the right on top,
 * that negated and the left added. */
static void leaf_narrow_insn(const Ins *insn, int op, Type narrow)
{
    static const unsigned char with_ix[] = { 0x86, 0x96, 0xa6, 0xb6, 0xae };
    int kind = op == TK_PLUS ? 0 : op == TK_MINUS ? 1 : op == TK_AMP ? 2
               : op == TK_PIPE ? 3 : 4, number;
    const Ent *left = &insn->in[0], *right = &insn->in[1];
    int rfwd = right->val >= 0 && vals[right->val].fwd;
    int lfwd = left->val >= 0 && vals[left->val].fwd;

    if (op == TK_SHL || op == TK_SHR) {
        (void) leaf_const(right, &number);
        if (!leaf_byte_to_a(left)) {
            leaf_load(left, R_HL);
            ld_a_l();
        }
        while (number--) {
            out_byte(0xcb);
            out_byte(op == TK_SHL ? 0x27 : type_unsigned(narrow) ? 0x3f : 0x2f);
        }                                   /* sla a, srl a, sra a */
    } else if (rfwd) {
        (void) leaf_byte_to_a(right);       /* the top */
        if (op == TK_MINUS) {
            out_byte2(0xed, 0x44);          /* neg: left - right = -right + left */
            kind = 0;
        }
        if (lfwd && leaf_nstacked && leaf_stacked_af[leaf_nstacked - 1]) {
            leaf_nstacked--;
            pop_rr(R_DE);
            out_byte(with_ix[kind] - 4);    /* op d */
            leaf_depth--;
        } else if (lfwd) {
            (void) leaf_pop(R_DE, 1);
            out_byte(with_ix[kind] - 3);    /* op e */
            leaf_depth--;
        } else {
            leaf_alu_byte(with_ix[kind], left);
        }
    } else {
        if (!leaf_byte_to_a(left)) {
            if (leaf_const(left, &number)) {
                out_byte2(0x3e, number & 0xff);     /* ld a, n */
            } else {
                leaf_load(left, R_HL);
                ld_a_l();
            }
        }
        leaf_alu_byte(with_ix[kind], right);
    }
    leaf_result_a(insn->res, narrow);
}

/* An int read through IY into a value whose home is IY too -- p = p->next
 * -- as ld iy, (iy+d), where the pointer read through dies there (`dies`):
 * IY is both. Answers whether it was. */
static int leaf_iy_read(int val, int disp, int dies)
{
    if (!dies || val < 0 || !vals[val].used || vals[val].fwd
        || !leaf_in_iy(&(Ent) { val, { 0 }, 0 }))
        return 0;
    out_byte3(0xfd, 0x37, disp);        /* ld iy, (iy+d): not fd 31, which
                                         * is ld ix, (iy+d) */
    vals[val].used = 0;                 /* there: leaf_result notes it only */
    leaf_result(val);
    vals[val].used = 1;

    return 1;
}

/* A conversion of the byte in A on the stack's top, to a byte -- the byte
 * is the same, and now of that type -- or to an int, which widening as the
 * byte's own type makes. Not to a _Bool, which is a test. */
static int leaf_keep_a(const Ins *insn, Type to, int result)
{
    Type type = leaf_a_type;
    int val = insn->in[0].val;

    /* A byte in a slot or in BC made a byte: into A, and on from there. */
    if (val >= 0 && !vals[val].fwd && type_size(to) == 1 && to != TY_BOOL
        && type_size(vals[val].type) == 1 && vals[val].type != TY_BOOL
        && leaf_byte_ok(&insn->in[0])) {
        (void) leaf_byte_to_a(&insn->in[0]);
        leaf_result_a(result, to);
        return 1;
    }
    if (!leaf_top_in_a || insn->in[0].val < 0 || !vals[insn->in[0].val].fwd
        || to == TY_BOOL
        || (type_size(to) != 1 && type_size(to) != ACC_INT_SIZE))
        return 0;
    if (type_size(to) == 1)
        type = to;
    leaf_depth--;
    leaf_top_in_hl = leaf_top_in_a = 0;
    leaf_result_a(result, type);

    return 1;
}

/* Whether an operand's byte can be had in A without HL: the stack's top,
 * or a value in BC or a slot of its own. */
static int leaf_byte_ok(const Ent *ent)
{
    return ent->val >= 0
           && (vals[ent->val].fwd || vals[ent->val].reg == R_BC
               || (vals[ent->val].reg == HOME_SLOT
                   && !(iy_web >= 0 && vals[ent->val].slot == fixed_slot[iy_web])));
}

/* A byte's AND, OR or XOR -- the answer a byte, as leaf_bitwise found --
 * in A: the side on top of the stack, or the left, into A where it is not
 * there already, and the other from where it is -- a constant, (ix+d), C,
 * or under it on the stack. */
static int leaf_byte_op(const Ins *insn, int op)
{
    static const unsigned char with_n[] = { 0xe6, 0xf6, 0xee };
    static const unsigned char with_ix[] = { 0xa6, 0xb6, 0xae };
    static const unsigned char with_c[] = { 0xa1, 0xb1, 0xa9 };
    static const unsigned char with_e[] = { 0xa3, 0xb3, 0xab };
    int kind = op == TK_AMP ? 0 : op == TK_PIPE ? 1 : 2, number;
    int rfwd = insn->in[1].val >= 0 && vals[insn->in[1].val].fwd;
    const Ent *in_a = rfwd ? &insn->in[1] : &insn->in[0];
    const Ent *other = rfwd ? &insn->in[0] : &insn->in[1];

    if (!leaf_byte_ok(in_a))
        return 0;
    if (leaf_const(other, &number)) {
        (void) leaf_byte_to_a(in_a);
        out_byte2(with_n[kind], number & 0xff);
    } else if (other->val >= 0 && vals[other->val].fwd) {
        (void) leaf_byte_to_a(in_a);            /* the top */
        if (leaf_nstacked && leaf_stacked_af[leaf_nstacked - 1]) {
            leaf_nstacked--;
            pop_rr(R_DE);                       /* the one under it, in D */
            out_byte(with_e[kind] - 1);         /* and d, or d, xor d */
        } else {
            (void) leaf_pop(R_DE, 1);           /* in E */
            out_byte(with_e[kind]);
        }
        leaf_depth--;
    } else if (other->val >= 0 && vals[other->val].reg == R_BC) {
        (void) leaf_byte_to_a(in_a);
        out_byte(with_c[kind]);
    } else if (leaf_byte_ok(other)) {
        (void) leaf_byte_to_a(in_a);
        frame_byte(with_ix[kind], vals[other->val].slot);
    } else {
        return 0;
    }
    leaf_hl_width = 1;
    leaf_result_a(insn->res, TY_UCHAR);

    return 1;
}

/* HL times `by`, BC kept: by doublings and additions where leaf_mul_ok
 * says, else by the helper, BC the constant. DE is lost. */
static void leaf_times(int by)
{
    if (leaf_mul_ok(by)) {
        leaf_mul(by);
        return;
    }
    push_rr(R_BC);
    ld_rr_imm(R_BC, by);
    rt_call(RT_MUL);
    pop_rr(R_BC);
}

/* A pointer and an int, the pointer's step past 4 -- a struct's size -- the
 * int scaled by it, the pointer waiting on the stack; `scaled` which side
 * is the pointer, 1 the left, 0 the right; or, `scaled` -1, two pointers'
 * difference in bytes divided by it, by the helper. BC kept. */
static void leaf_wide_step(const Ins *insn, int op, int step, int scaled)
{
    (void) leaf_operands_in(insn, 0);   /* left in HL, right in DE */
    if (scaled < 0) {
        or_a_a();
        sbc_hl_rr(R_DE);
        push_rr(R_BC);
        ld_rr_imm(R_BC, step);
        rt_call(RT_DIVS);
        pop_rr(R_BC);
    } else {
        if (scaled == 1)
            ex_de_hl();                 /* the int into HL, the pointer DE */
        push_rr(R_DE);
        leaf_times(step);
        pop_rr(R_DE);
        if (scaled == 1 && op == TK_MINUS) {
            ex_de_hl();                 /* pointer less the scaled int */
            or_a_a();
            sbc_hl_rr(R_DE);
        } else {
            add_hl_rr(R_DE);
        }
    }
    leaf_result(insn->res);
}

/* An operator the first pass's helpers make, HL and BC to HL, every other
 * register kept: the left in HL, the right moved to BC through the stack
 * and BC kept around the call, or used where it is if it lives there. */
static void leaf_helper(const Ins *insn, int which)
{
    if (leaf_operands_in(insn, 1) == R_BC) {
        rt_call(which);
    } else {
        push_rr(R_BC);
        push_rr(R_DE);
        pop_rr(R_BC);
        rt_call(which);
        pop_rr(R_BC);
    }
    leaf_result(insn->res);
}

/* Whether HL times `by` is doublings and additions, at most MUL_MAX_STEPS
 * of them, as the first pass makes it: 0 and 1 are folded before this. */
static int leaf_mul_ok(int by)
{
    int top = 0, bits = 0, rest;

    if (by <= 1 || by > 0xffff)
        return 0;
    for (rest = by; rest > 1; rest >>= 1)
        top++;
    for (rest = by; rest; rest &= rest - 1)
        bits++;

    return top + bits - 1 <= MUL_MAX_STEPS;
}

/* HL times `by`, leaf_mul_ok's: a power of two doubled, anything else with
 * the value in DE, added at each bit below the top as HL is doubled. */
static void leaf_mul(int by)
{
    int bit = 0;

    while ((1 << (bit + 1)) <= by)
        bit++;
    if ((by & (by - 1)) == 0) {
        while (bit--)
            add_hl_hl();
        return;
    }
    push_rr(R_HL);
    pop_rr(R_DE);
    while (bit--) {
        add_hl_hl();
        if (by & (1 << bit))
            add_hl_rr(R_DE);
    }
}

/* AND, OR and XOR wider than a byte, which leaf_insn's byte forms do not
 * make: a constant's bytes where the top one is left alone; both sides two
 * bytes wide or less -- for an AND either -- a byte at a time against DE;
 * and the rest by the first pass's helper, HL and BC to HL, BC kept.
 * Returns 0 where the byte forms do it. */
static int leaf_bitwise(const Ins *insn, int op, int byte_op)
{
    int left = leaf_known_width(&insn->in[0]), right = leaf_known_width(&insn->in[1]);
    int which = leaf_const(&insn->in[1], &(int) { 0 }) ? 1 : 0, value;

    if (op == TK_AMP ? left == 1 || right == 1 : left == 1 && right == 1)
        return 0;
    leaf_hl_width = op == TK_AMP ? (left < right ? left : right)
                                 : (left > right ? left : right);
    if (leaf_const(&insn->in[which], &value) && leaf_const_bytes(op, value, 0)) {
        leaf_operand_hl(insn, 1 - which);
        leaf_const_bytes(op, value, 1);
    } else if (op == TK_AMP ? left <= 2 || right <= 2 : left <= 2 && right <= 2) {
        (void) leaf_operands_in(insn, 0);       /* the right in DE */
        if (op == TK_AMP) {
            ld_a_h();
            out_byte(byte_op - 1);              /* and d */
            out_byte(0x57);                     /* ld d, a */
            ld_a_l();
            out_byte(byte_op);                  /* and e */
            out_byte(0x5f);                     /* ld e, a */
            sbc_hl_hl();                        /* and cleared the carry */
            out_byte2(0x6b, 0x62);              /* ld l, e; ld h, d */
        } else {
            ld_a_l();
            out_byte(byte_op);                  /* or e, xor e */
            ld_l_a();
            ld_a_h();
            out_byte(byte_op - 1);              /* or d, xor d */
            ld_h_a();
        }
    } else {
        leaf_helper(insn, op == TK_AMP ? RT_AND : op == TK_PIPE ? RT_OR : RT_XOR);
        return 1;
    }
    leaf_result(insn->res);

    return 1;
}

/* A comparison: flags and the condition that says it is true, from HL less
 * DE, both moved by 0x800000 when signed. */
/* A value's byte into A, where it has one: a narrow value in BC or a slot,
 * or in HL. Answers whether it could. */
/* Whether `val` is read by one instruction alone, and by no phi: then the
 * slot it was stored to just now is read by nothing else. */
static int *read_counts;         /* by value: its reads, -1 by a phi */

static int leaf_read_once(int val)
{
    int at, operand, phi, pred;

    if (!read_counts_made) {
        read_counts = realloc(read_counts, ((size_t) nvals + 1) * sizeof *read_counts);
        if (!read_counts)
            acc_error("out of memory for the SSA form");
        memset(read_counts, 0, ((size_t) nvals + 1) * sizeof *read_counts);
        for (at = 0; at != ninsns; at++)
            for (operand = 0; operand != insns[at].nin; operand++)
                if (insns[at].in[operand].val >= 0)
                    read_counts[insns[at].in[operand].val]++;
        for (phi = 0; phi != nphis; phi++)
            for (pred = 0; phis[phi].live && pred != preds[phis[phi].block].count;
                 pred++)
                if (phis[phi].in[pred] >= 0)
                    read_counts[phis[phi].in[pred]] = -1;
        read_counts_made = 1;
    }

    return val >= 0 && val < nvals && read_counts[val] == 1;
}

static int leaf_byte_to_a(const Ent *ent)
{
    int val = ent->val;

    if (val < 0)
        return 0;
    if (vals[val].fwd && leaf_top_in_a) {
        leaf_depth--;
        leaf_top_in_hl = leaf_top_in_a = 0;     /* in A already */
    } else if (vals[val].fwd && leaf_top_in_hl) {
        leaf_depth--;
        leaf_top_in_hl = 0;
        ld_a_l();
    } else if (vals[val].fwd) {
        if (leaf_pop(R_HL, 1) != R_AF_BYTE)     /* pop af, or pop hl */
            ld_a_l();
        leaf_depth--;
    } else if (vals[val].reg == R_BC) {
        out_byte(0x79);                         /* ld a, c */
    } else if (vals[val].reg == HOME_SLOT && !(iy_web >= 0
               && vals[val].slot == fixed_slot[iy_web])) {
        if (!(leaf_a_stored_at == out_here() && leaf_a_stored_slot == vals[val].slot
              && leaf_a_stored_epoch == out_rewinds && join_at != out_here()))
            ld_a_ix(vals[val].slot);    /* not stored there just now */
        else if (leaf_read_once(val)) {
            out_rewind(leaf_a_store_from);  /* stored for this read alone */
            leaf_a_stored_at = -1;
        }
    } else {
        return 0;
    }

    return 1;
}

/* Whether BC holds a value at instruction `at`: one that lives there and
 * is made by then and read after -- its interval, end to end. */
static int *bc_busy;             /* by instruction: the values in BC */

static int leaf_bc_busy(int at)
{
    int val, pos, run;

    /* Each interval added once, as +1 at its start and -1 past its end,
     * and the sums run along. */
    if (!bc_busy_made) {
        bc_busy = realloc(bc_busy, ((size_t) ninsns + 2) * sizeof *bc_busy);
        if (!bc_busy)
            acc_error("out of memory for the SSA form");
        memset(bc_busy, 0, ((size_t) ninsns + 2) * sizeof *bc_busy);
        for (val = 0; val != nvals; val++) {
            int from = vals[val].def, to = vals[val].last;

            if (vals[val].reg != R_BC || !vals[val].used || vals[val].fwd)
                continue;
            from = from < 0 ? 0 : from;
            to = to >= ninsns ? ninsns - 1 : to;
            if (from > to)
                continue;
            bc_busy[from]++;
            bc_busy[to + 1]--;
        }
        for (pos = 0, run = 0; pos != ninsns; pos++) {
            run += bc_busy[pos];
            bc_busy[pos] = run;
        }
        bc_busy_made = 1;
    }

    return at >= 0 && at < ninsns && bc_busy[at] > 0;
}

static int leaf_compare(const Ins *insn, int op)
{
    int number;

    /* A byte against a constant in its range: in A, a cp -- a signed char
     * moved by 0x80 first, which makes the order an unsigned one. */
    if (insn->in[0].val >= 0 && leaf_const(&insn->in[1], &number)
        && type_size(leaf_held(&insn->in[0])) == 1
        && !(insn->in[0].val >= 0 && !vals[insn->in[0].val].fwd
             && iy_web >= 0 && vals[insn->in[0].val].slot == fixed_slot[iy_web]
             && vals[insn->in[0].val].reg == HOME_SLOT)) {
        Type held = leaf_held(&insn->in[0]);
        int is_signed = !type_unsigned(held), low = is_signed ? -128 : 0;
        int high = is_signed ? 127 : 255;
        int bias = is_signed && op != TK_EQ && op != TK_NE ? 0x80 : 0;
                                        /* equal is equal, either way */
        int want = op == TK_LE || op == TK_GT ? number + 1 : number;

        if (number >= low && number <= high && want <= high
            && leaf_byte_to_a(&insn->in[0])) {
            if (bias)
                out_byte2(0xee, bias);          /* xor n */
            if (((want + bias) & 0xff) == 0)
                or_a_a();               /* cp 0: the same flags */
            else
                cp_a_imm(want + bias);

            return op == TK_LT || op == TK_LE ? JP_C : op == TK_GE || op == TK_GT
                   ? JP_NC : op == TK_EQ ? JP_Z : JP_NZ;
        }
    }

    /* Against 0 for equality: the byte in A where the value is one, the
     * pair tested where it is not -- add hl, bc and back, BC as it was. */
    if ((op == TK_EQ || op == TK_NE) && leaf_const(&insn->in[1], &number)
        && number == 0) {
        const Ent *ent = &insn->in[0];

        int byte = leaf_width(ent) == 1 || leaf_held(ent) == TY_UCHAR
                   || leaf_held(ent) == TY_BOOL || leaf_known_width(ent) == 1;

        if (byte && leaf_byte_ok(ent)) {
            (void) leaf_byte_to_a(ent);         /* ld a, c; or in A already */
        } else {
            leaf_operand_hl(insn, 0);
            if (byte) {
                ld_a_l();
            } else {
                add_hl_rr(R_BC);
                or_a_a();
                sbc_hl_rr(R_BC);
                return op == TK_EQ ? JP_Z : JP_NZ;
            }
        }
        or_a_a();

        return op == TK_EQ ? JP_Z : JP_NZ;
    }

    int is_signed = !type_unsigned(insn->in[0].attr.type)
                    && !type_unsigned(insn->in[1].attr.type)
                    && op != TK_EQ && op != TK_NE;
    int right;

    /* Unsigned, with the left side in BC and the right not: the right into
     * HL and BC taken from it where it is, the comparison turned about. */
    if (!is_signed && insn->in[0].val >= 0 && !vals[insn->in[0].val].fwd
        && vals[insn->in[0].val].reg == R_BC
        && !(insn->in[1].val >= 0 && vals[insn->in[1].val].reg == R_BC)) {
        leaf_operand_hl(insn, 1);
        leaf_top_in_hl = 0;
        op = mirror(op);
        if (op == TK_LE || op == TK_GT)
            out_byte(0x37);                     /* scf: HL - BC - 1 */
        else
            or_a_a();
        sbc_hl_rr(R_BC);

        return op == TK_LT || op == TK_LE ? JP_C : op == TK_GE || op == TK_GT
               ? JP_NC : op == TK_EQ ? JP_Z : JP_NZ;
    }
    if (is_signed && leaf_const(&insn->in[1], &number)) {
        leaf_operand_hl(insn, 0);
        ld_rr_imm(R_DE, 0x800000);
        add_hl_rr(R_DE);
        ld_rr_imm(R_DE, (number + 0x800000) & 0xffffff);
        if (op == TK_LE || op == TK_GT)
            out_byte(0x37);
        else
            or_a_a();
        sbc_hl_rr(R_DE);

        return op == TK_LT || op == TK_LE ? JP_C : op == TK_GE || op == TK_GT
               ? JP_NC : op == TK_EQ ? JP_Z : JP_NZ;
    }
    right = leaf_operands_in(insn, 1);
    if (op == TK_LE || op == TK_GT)
        out_byte(0x37);                         /* scf: HL - DE - 1 */
    else
        or_a_a();
    sbc_hl_rr(right);
    if (is_signed)
        signed_carry();

    return op == TK_LT || op == TK_LE ? JP_C : op == TK_GE || op == TK_GT ? JP_NC
           : op == TK_EQ ? JP_Z : JP_NZ;
}

/* A 0 or 1 into HL from flags `cc` true. */
static void leaf_truth_value(int cc)
{
    ld_rr_imm(R_HL, 0);
    out_byte2(0x20 + ((cc ^ 0x08) & 0x18), 1);  /* jr !cc, past */
    inc_hl();
    leaf_hl_type = TY_BOOL;
}

/* Whether an operand is 0 or 1 already: a constant that is, or a value
 * held as _Bool -- a comparison's answer, or a _Bool read. */
static int leaf_is_01(const Ent *ent)
{
    int number;

    if (leaf_const(ent, &number))
        return number == 0 || number == 1;

    return leaf_held(ent) == TY_BOOL;
}

/* The value in HL, `from`, made a `to`: a _Bool is 0 or 1 by whether the
 * value is 0 -- 256 is true, not its low byte -- and anything narrower cut
 * down, where it is not in range already. */
static void leaf_convert(const Ent *from, Type to)
{
    if (to == TY_BOOL && !leaf_is_01(from)) {
        add_hl_rr(R_BC);
        or_a_a();
        sbc_hl_rr(R_BC);                /* Z when HL is 0, BC as it was */
        leaf_truth_value(JP_NZ);
    } else if (!leaf_fits(leaf_held(from), to)) {
        leaf_narrow(to);
    } else {
        leaf_hl_type = leaf_held(from);
    }
}

/* A cached local's step written through to its memory, from where the
 * new value is: HL, BC, or IY (-1). HL is kept. */
static void cache_write(const Ins *insn, int reg)
{
    int slot = inline_moved((int) insn->rec->arg[0]);

    if (!insn->mem_store)
        return;
    if (reg == R_HL) {
        ld_ix_rr(slot, R_HL);
        return;
    }
    if (disp_fits(slot)) {
        out_byte3(0xdd, reg == R_BC ? 0x0f : 0x3e, slot);  /* ld (ix+d), bc/iy */
        return;
    }
    push_rr(R_HL);
    if (reg == R_BC)
        push_rr(R_BC);
    else
        out_byte2(0xfd, 0xe5);          /* push iy */
    pop_rr(R_HL);
    ld_ix_rr(slot, R_HL);
    pop_rr(R_HL);
}

/* An int from the frame slot at `disp` made `val`, where val lives in BC or
 * IY: loaded there straight, ld bc, (ix+d) or ld iy, (ix+d), not through HL
 * and the stack. Returns 0, having made nothing, for any other home. */
static int leaf_slot_to_home(int val, int disp, Type type)
{
    int to_iy;

    if (val < 0 || !vals[val].used || vals[val].fwd
        || type_size(vals[val].type) != ACC_INT_SIZE || !disp_fits(disp))
        return 0;
    to_iy = vals[val].reg == HOME_SLOT && iy_web >= 0
            && vals[val].slot == fixed_slot[iy_web];
    if (vals[val].reg != R_BC && !to_iy)
        return 0;
    frame_byte(to_iy ? 0x31 : 0x07, disp);         /* ld iy / bc, (ix+d) */
    if (leaf_holds) {
        leaf_holds[val] = def_at[val] >= 0 ? type : TY_VOID;
        leaf_widths[val] = 3;
    }

    return 1;
}

/* One operand of a delegated instruction onto the classic stack: a
 * constant made again, a value in its slot -- or IY's, which the first
 * pass's code reads as its local in IY -- or in BC as it is, the one left
 * on the stack here from HL, and a global's address through DE. */
static int hl_taken;             /* leaf_delegate: an operand is in HL */

static void leaf_delegate_operand(const Ent *ent, int fwd_reg)
{
    int val = ent->val;

    if (val < 0 || (vals[val].reg == HOME_SLOT && !vals[val].fwd)) {
        load(ent);
        return;
    }
    if (vals[val].fwd) {
        vpush_reg(fwd_reg);             /* leaf_delegate put it there */
    } else if (vals[val].reg == R_BC) {
        vpush_reg(R_BC);
    } else {
        int reg = fwd_reg == R_HL && !hl_taken ? R_HL : R_DE;

        leaf_load(ent, reg);
        vpush_reg(reg);
        hl_taken |= reg == R_HL;
    }
    (vsp - 1)->type = ent->attr.type;
    vset_ext(ent->attr.ext);
    vset_quals(ent->attr.quals);
}

/* An instruction with a wide value in it, made by the first pass's code:
 * its operands on the classic stack, the call made again, and the answer
 * -- a wide one into its slot, anything narrower into HL as the code here
 * makes it. BC, which that code may take for itself, is saved around it
 * where a value is in it; IY is the first pass's own local, which its code
 * saves where it uses IY. */
static void leaf_delegate(const Ins *insn, int at)
{
    int operand, keep_bc, res = insn->res, nfwd = 0, global = 0, pushed_at;
    int fwd_reg[2] = { R_HL, R_HL };

    /* What is on the stack here: one into HL, or two -- a store's address
     * and its value -- the left into HL and the right into DE. */
    for (operand = 0; operand != insn->nin; operand++) {
        int val = insn->in[operand].val;

        nfwd += val >= 0 && vals[val].fwd;
        global |= val >= 0 && !vals[val].fwd && vals[val].reg == HOME_GLOBAL;
    }
    if (nfwd > 2 || (nfwd == 2 && (insn->nin != 2 || global))) {
        fail = "internal: a wide value's instruction with too much on the stack";
        return;
    }
    if (nfwd == 2) {
        (void) leaf_operands_in(insn, 0);
        fwd_reg[1] = R_DE;
    } else {
        for (operand = 0; operand != insn->nin; operand++)
            if (insn->in[operand].val >= 0 && vals[insn->in[operand].val].fwd) {
                leaf_operand_hl(insn, operand);     /* the one on the stack */
                break;
            }
    }
    if (vtop != 0) {
        fail = "internal: the classic stack not empty for a wide value";
        return;
    }
    hl_taken = nfwd != 0;
    for (operand = 0; operand != insn->nin && !fail; operand++)
        leaf_delegate_operand(&insn->in[operand], fwd_reg[operand < 2 ? operand : 1]);

    /* What the code here loads is loaded; what the first pass makes from
     * here on may take BC. */
    keep_bc = leaf_bc_busy(at);
    pushed_at = out_here();
    if (keep_bc)
        push_rr(R_BC);
    switch (insn->op) {
    case I_CONV:
        vconvert(insn->local_type);
        break;
    case I_STEP:
        vpush_const(1, TY_INT);
        vapply((unsigned char) insn->step_op, type_narrow(insn->local_type));
        vconvert(insn->local_type);
        break;
    default:
        call(insn);
        break;
    }
    if (res < 0 || !vals[res].used) {
        while (vtop)
            vdrop();
    } else if (type_wide(vals[res].type) || type_float(vals[res].type)) {
        vstore_local(vals[res].slot, vals[res].type);
        vdrop();
    } else {
        int reg = vpop_reg();

        if (reg != R_HL)
            mov_rr(R_HL, reg);
        leaf_hl_type = vals[res].type;
    }
    if (vtop != 0)
        fail = "internal: the classic stack not empty after a wide value";
    if (keep_bc && out_here() == pushed_at + 1)
        out_rewind(pushed_at);          /* nothing made: nothing to keep */
    else if (keep_bc)
        pop_rr(R_BC);
    if (res >= 0 && vals[res].used
        && !(type_wide(vals[res].type) || type_float(vals[res].type)))
        leaf_result(res);
}

/* A long operand's address into HL or DE: its slot's, or the pool's. */
static void leaf_wide_addr(const Ent *ent, int reg)
{
    if (ent->val == S_CONST)
        pool_address(reg, leaf_wide_bits(ent), 4);
    else
        lea_rr_ix(reg, vals[ent->val].slot);
}

/* A long operand's four bytes into the slot at `to`, where they are not
 * there already. */
static void leaf_wide_copy(const Ent *ent, int to)
{
    if (ent->val == S_CONST) {
        uint32_t bits = leaf_wide_bits(ent);

        ld_rr_imm(R_HL, (int) (bits & 0xffffff));
        ld_ix_rr(to, R_HL);
        frame_byte(0x36, to + 3);               /* ld (ix+d), n */
        out_byte((int) (bits >> 24));
        return;
    }
    if (vals[ent->val].slot == to)
        return;
    ld_rr_ix(R_HL, vals[ent->val].slot);
    ld_ix_rr(to, R_HL);
    ld_a_ix(vals[ent->val].slot + 3);
    ld_ix_a(to + 3);
}

/* A long operator made in line rather than by the runtime's routine: add
 * and subtract through HL and A; &, | and ^ with a constant, a byte at a
 * time, a byte of 0 or 0xff costing less or nothing; a shift by a
 * constant, whole bytes moved and the bits left over through HL and A
 * (left) or in the slot (right). Counted first -- wb_emit clear -- so
 * that it is made only where it is no bigger than the call, or a little
 * bigger in a loop, where it is several times as fast. */
static int wb_emit, wb_n;

static void wb(int byte)
{
    if (wb_emit)
        out_byte(byte);
    wb_n++;
}

static void wb_ix(int op, int disp)             /* dd op d */
{
    wb(0xdd);
    wb(op);
    wb(disp & 0xff);
}

static void wb_ix_n(int disp, int n)            /* ld (ix+d), n */
{
    wb_ix(0x36, disp);
    wb(n & 0xff);
}

static void wb_cb_ix(int op, int disp)          /* dd cb d op */
{
    wb(0xdd);
    wb(0xcb);
    wb(disp & 0xff);
    wb(op);
}

static void wb_imm(int op, int value)           /* op nn, three bytes */
{
    wb(op);
    wb(value & 0xff);
    wb((value >> 8) & 0xff);
    wb((value >> 16) & 0xff);
}

/* How much bigger the line may be than the call, in a loop. */
#define WIDE_LOOP_SLACK 12

static int leaf_wide_line(const Ent *left, const Ent *right, int to, int arith,
                          Type type)
{
    int lslot = left->val >= 0 ? vals[left->val].slot : 0;
    int rslot = right->val >= 0 ? vals[right->val].slot : 0;
    int lconst = left->val == S_CONST, rconst = right->val == S_CONST;
    uint32_t lbits = lconst ? leaf_wide_bits(left) : 0;
    uint32_t rbits = rconst ? leaf_wide_bits(right) : 0;
    int i, n, k, r, sign = !type_unsigned(type);

    switch (arith) {
    case TK_PLUS: case TK_MINUS:
        if (lconst)
            wb_imm(0x21, (int) (lbits & 0xffffff));     /* ld hl, nn */
        else
            wb_ix(0x27, lslot);                         /* ld hl, (ix+d) */
        if (rconst)
            wb_imm(0x11, (int) (rbits & 0xffffff));     /* ld de, nn */
        else
            wb_ix(0x17, rslot);                         /* ld de, (ix+d) */
        if (arith == TK_PLUS) {
            wb(0x19);                                   /* add hl, de */
        } else {
            wb(0xb7);                                   /* or a, a */
            wb(0xed);
            wb(0x52);                                   /* sbc hl, de */
        }
        wb_ix(0x2f, to);                                /* ld (ix+d), hl */
        if (lconst) {
            wb(0x3e);                                   /* ld a, n */
            wb((int) (lbits >> 24));
        } else {
            wb_ix(0x7e, lslot + 3);                     /* ld a, (ix+d) */
        }
        if (rconst) {
            wb(arith == TK_PLUS ? 0xce : 0xde);         /* adc/sbc a, n */
            wb((int) (rbits >> 24));
        } else {
            wb_ix(arith == TK_PLUS ? 0x8e : 0x9e, rslot + 3);
        }
        wb_ix(0x77, to + 3);                            /* ld (ix+d), a */
        return 1;
    case TK_AMP: case TK_PIPE: case TK_CARET:
        if (!rconst || lconst)
            return 0;
        for (i = 0; i != 4; i++) {
            int b = (int) (rbits >> (8 * i)) & 0xff;
            int keep = arith == TK_AMP ? b == 0xff : b == 0;
            int fill = arith == TK_AMP ? b == 0 : arith == TK_PIPE && b == 0xff;

            if (keep && lslot == to)
                continue;
            if (fill) {
                wb_ix_n(to + i, arith == TK_AMP ? 0 : 0xff);
                continue;
            }
            wb_ix(0x7e, lslot + i);                     /* ld a, (ix+d) */
            if (arith == TK_CARET && b == 0xff) {
                wb(0x2f);                               /* cpl */
            } else if (!keep) {
                wb(arith == TK_AMP ? 0xe6 : arith == TK_PIPE ? 0xf6 : 0xee);
                wb(b);
            }
            wb_ix(0x77, to + i);                        /* ld (ix+d), a */
        }
        return 1;
    case TK_SHL: case TK_SHR:
        if (!rconst || lconst)
            return 0;
        n = (int) (rbits & 31);
        k = n / 8;
        r = n % 8;
        if (arith == TK_SHL) {
            if (k == 0) {                               /* the bits, in HL and A */
                wb_ix(0x27, lslot);
                wb_ix(0x7e, lslot + 3);
            } else {
                if (k == 1) {
                    wb_ix(0x27, lslot);                 /* L0..L2 to t1..t3 */
                    wb_ix(0x2f, to + 1);
                } else if (k == 2) {
                    wb_ix(0x27, lslot - 1);             /* L0, L1 to t2, t3 */
                    wb_ix(0x2f, to + 1);
                    wb_ix_n(to + 1, 0);
                } else {
                    wb_ix(0x7e, lslot);                 /* L0 to t3 */
                    wb_ix(0x77, to + 3);
                    wb_imm(0x21, 0);
                    wb_ix(0x2f, to);
                }
                if (k != 3)
                    wb_ix_n(to, 0);
                if (!r)
                    return 1;
                wb_ix(0x27, to);
                wb_ix(0x7e, to + 3);
            }
            for (i = 0; i != r; i++) {
                wb(0x29);                               /* add hl, hl */
                wb(0x17);                               /* rla */
            }
            wb_ix(0x2f, to);
            wb_ix(0x77, to + 3);
            return 1;
        }
        if (k == 0) {
            if (lslot != to) {
                wb_ix(0x27, lslot);
                wb_ix(0x2f, to);
                wb_ix(0x7e, lslot + 3);
                wb_ix(0x77, to + 3);
            }
        } else if (k == 1) {
            wb_ix(0x27, lslot + 1);                     /* L1..L3 to t0..t2 */
            wb_ix(0x2f, to);
            if (sign) {
                wb_ix(0x7e, lslot + 3);
                wb(0x17);                               /* rla */
                wb(0x9f);                               /* sbc a, a */
                wb_ix(0x77, to + 3);
            } else {
                wb_ix_n(to + 3, 0);
            }
        } else if (k == 2) {
            wb_ix(0x27, lslot + 2);                     /* L2, L3 to t0, t1 */
            wb_ix(0x2f, to);
            if (sign) {
                wb_ix(0x7e, to + 1);
                wb(0x17);
                wb(0x9f);
                wb_ix(0x77, to + 2);
                wb_ix(0x77, to + 3);
            } else {
                wb_ix_n(to + 2, 0);
                wb_ix_n(to + 3, 0);
            }
        } else {
            wb_ix(0x7e, lslot + 3);                     /* L3 to t0 */
            wb_ix(0x77, to);
            if (sign) {
                wb(0x17);
                wb(0x9f);
                wb_ix(0x77, to + 1);
                wb_ix(0x77, to + 2);
                wb_ix(0x77, to + 3);
            } else {
                wb_imm(0x21, 0);
                wb_ix(0x2f, to + 1);
            }
        }
        for (i = 0; i != r; i++) {
            wb_cb_ix(sign ? 0x2e : 0x3e, to + 3);       /* sra / srl */
            wb_cb_ix(0x1e, to + 2);                     /* rr */
            wb_cb_ix(0x1e, to + 1);
            wb_cb_ix(0x1e, to);
        }
        return 1;
    }

    return 0;
}

/* Whether a long operator was made in line: counted, weighed against the
 * call's bytes -- the left copied to the answer, its address and the
 * right's, the call -- and made if it wins. */
static int leaf_wide_inline(const Ent *left, const Ent *right, int to, int arith,
                            Type type, int blk)
{
    int lslot = left->val >= 0 ? vals[left->val].slot : 0;
    int rslot = right->val >= 0 ? vals[right->val].slot : 0;
    int call_bytes;

    if (!disp_fits(to - 1) || !disp_fits(to + 3)
        || (left->val >= 0 && (!disp_fits(lslot - 1) || !disp_fits(lslot + 3)))
        || (right->val >= 0 && (!disp_fits(rslot) || !disp_fits(rslot + 3))))
        return 0;
    call_bytes = (left->val >= 0 && lslot == to ? 0 : left->val == S_CONST ? 11 : 12)
                 + 3 + (right->val == S_CONST ? 4 : 3) + 4;
    wb_emit = 0;
    wb_n = 0;
    if (!leaf_wide_line(left, right, to, arith, type))
        return 0;
    if (wb_n > call_bytes + (loop_depth[blk] ? WIDE_LOOP_SLACK : 0))
        return 0;
    wb_emit = 1;
    (void) leaf_wide_line(left, right, to, arith, type);

    return 1;
}

/* An instruction with a long in it, made here (leaf_wide_ok). */
static void leaf_wide(const Ins *insn, int blk)
{
    int to = insn->res >= 0 ? vals[insn->res].slot : 0, at;
    Type from, type;

    switch (insn->op) {
    case GL_vapply: {
        int arith = (int) insn->rec->arg[0], stacked;
        int which = leaf_wide_helper(arith, vals[insn->res].type);
        const Ent *left = &insn->in[0], *right = &insn->in[1];

        if (!vals[insn->res].used)
            return;
        /* The right where the answer goes: swapped where the order does
         * not matter, as copying the left there would lose it. */
        if (right->val >= 0 && vals[right->val].slot == to
            && !(left->val >= 0 && vals[left->val].slot == to)
            && (arith == TK_PLUS || arith == TK_AMP || arith == TK_PIPE
                || arith == TK_CARET || arith == TK_STAR)) {
            left = &insn->in[1];
            right = &insn->in[0];
        }
        /* A constant on the left, where the order does not matter: to the
         * right, where the line and the pool can have it. */
        if (left->val == S_CONST && right->val != S_CONST
            && (arith == TK_PLUS || arith == TK_AMP || arith == TK_PIPE
                || arith == TK_CARET || arith == TK_STAR)) {
            const Ent *swap = left;

            left = right;
            right = swap;
        }
        if (leaf_wide_inline(left, right, to, arith, vals[insn->res].type, blk))
            return;

        /* The right read where it lies, or from the pool -- or, where it
         * is in the answer's slot still, the pool is full, or the routine
         * writes it (division, the remainder), from a copy pushed. */
        stacked = which == RT_LDIVU || which == RT_LDIVS || which == RT_LREMU
                  || which == RT_LREMS
                  || (right->val >= 0 && vals[right->val].slot == to
                      && !(left->val >= 0 && vals[left->val].slot == to))
                  || (right->val == S_CONST && !pool_room(leaf_wide_bits(right), 4));
        if (stacked) {
            if (right->val == S_CONST) {
                ld_rr_imm(R_HL, (int) (leaf_wide_bits(right) >> 24));
                push_rr(R_HL);
                ld_rr_imm(R_HL, (int) (leaf_wide_bits(right) & 0xffffff));
            } else {
                ld_rr_ix(R_HL, vals[right->val].slot + 3);
                push_rr(R_HL);
                ld_rr_ix(R_HL, vals[right->val].slot);
            }
            push_rr(R_HL);
        }
        leaf_wide_copy(left, to);
        if (stacked) {
            ld_rr_imm(R_HL, 0);
            out_byte(0x39);                     /* add hl, sp */
            ex_de_hl();
        } else {
            leaf_wide_addr(right, R_DE);
        }
        lea_rr_ix(R_HL, to);
        rt_call(which);
        if (stacked) {
            pop_rr(R_DE);
            pop_rr(R_DE);
        }
        return;
    }
    case I_CONV: case GL_vconvert: case GL_vcast:
        from = insn->in[0].attr.type;
        type = insn->op == I_CONV ? insn->local_type : (Type) insn->rec->arg[0];
        if (leaf_long(type) && leaf_long(from)) {
            if (vals[insn->res].used)
                leaf_wide_copy(&insn->in[0], to);
            return;
        }
        if (leaf_long(type)) {                  /* an int or narrower, widened */
            leaf_operand_hl(insn, 0);
            if (!vals[insn->res].used)
                return;
            ld_ix_rr(to, R_HL);
            if (type_unsigned(from) || type_pointer(from)) {
                frame_byte(0x36, to + 3);       /* ld (ix+d), 0 */
                out_byte(0);
            } else {
                add_hl_rr(R_HL);                /* the sign into carry */
                out_byte(0x9f);                 /* sbc a, a */
                ld_ix_a(to + 3);
            }
            return;
        }
        /* A long narrowed: its low bytes. */
        if (insn->in[0].val == S_CONST)
            ld_rr_imm(R_HL, (int) (leaf_wide_bits(&insn->in[0]) & 0xffffff));
        else
            ld_rr_ix(R_HL, vals[insn->in[0].val].slot);
        if (type_size(type) < ACC_INT_SIZE)
            leaf_narrow(type);
        leaf_hl_type = type;
        leaf_result(insn->res);
        return;
    case GL_vderef:
        leaf_operand_hl(insn, 0);
        if (!vals[insn->res].used)
            return;
        out_byte2(0xed, 0x17);                  /* ld de, (hl) */
        ld_ix_rr(to, R_DE);
        out_byte(0x23);                         /* inc hl */
        out_byte(0x23);
        out_byte(0x23);
        ld_a_hl();
        ld_ix_a(to + 3);
        return;
    case I_SET:
        leaf_wide_copy(&insn->in[0], vals[insn->target].slot);
        return;
    case I_BR:
        at = vals[insn->in[0].val].slot;
        ld_a_ix(at);                            /* Z when all four are 0 */
        frame_byte(0xb6, at + 1);               /* or a, (ix+d) */
        frame_byte(0xb6, at + 2);
        frame_byte(0xb6, at + 3);
        branch_to(JP_NZ, insn, blk);
        return;
    }
    fail = "internal: a long's instruction leaf_wide_ok let through";
}

static void leaf_insn(const Ins *insn, int blk, int at)
{
    int op = insn->op, number, value, cc;

    /* Between a comparison and the branch it was made with: nothing. */
    if ((at > fused_from && at < skip_branch) || at <= leaf_skip_until)
        return;

    /* A step in place, where the value and the new one share BC or IY:
     * HL is not touched. An unsigned char steps its byte alone, and wraps
     * as its type does; a pointer in IY steps by up to 127 with lea. */
    if (op == I_STEP && insn->in[0].val >= 0 && insn->kills & 1
        && ((vals[insn->res].reg == R_BC && vals[insn->in[0].val].reg == R_BC)
            || (leaf_in_iy(&insn->in[0])
                && leaf_in_iy(&(Ent) { insn->res, { 0 }, 0 })))
        && (insn->local_type == TY_UCHAR
            || (type_size(insn->local_type) == ACC_INT_SIZE
                && (!type_pointer(insn->local_type)
                    || (number = type_step(insn->local_type,
                                           insn->in[0].attr.ext)) == 1
                    || (leaf_in_iy(&insn->in[0]) && number > 1
                        && number <= 127))))
        && !(insn->local_type == TY_UCHAR && leaf_in_iy(&insn->in[0]))) {
        int count = insn->step_op == TK_MINUS ? -1 : 1;

        if (insn->local_type == TY_UCHAR)
            out_byte(count < 0 ? 0x0d : 0x0c);          /* dec c, inc c */
        else if (type_pointer(insn->local_type) && number > 1)
            out_byte3(0xed, 0x33, count * number & 0xff);   /* lea iy, iy+d */
        else
            step_reg(leaf_in_iy(&insn->in[0]) ? -1 : R_BC, count);
        cache_write(insn, leaf_in_iy(&insn->in[0]) ? -1 : R_BC);
        return;
    }

    /* HL made free for what uses it: not a frame call, a statement's end,
     * a constant made again where read, a string's bytes, or a drop of
     * nothing. */
    if (!(op == I_BR && at == skip_branch) && op != I_FRAME
        && op != GL_gen_stmt_end && op != GL_gen_value_end
        && op != GL_vpush_const && op != GL_vpush_bss && op != GL_gen_data
        && !(op == GL_vdrop && insn->nin == 0))
        leaf_free_hl(insn);
    if (insn->wide) {
        if (op == I_BR && at == skip_branch)
            return;
        leaf_wide(insn, blk);
        return;
    }
    if (insn->delegated && op != GL_vdrop) {
        leaf_delegate(insn, at);
        return;
    }
    switch (op) {
    case I_FRAME: case GL_vdrop: case GL_gen_stmt_end: case GL_gen_value_end:
        if (op == GL_vdrop) {
            int operand;

            for (operand = 0; operand != insn->nin; operand++)
                if (insn->in[operand].val >= 0 && vals[insn->in[operand].val].fwd) {
                    if (!leaf_top_in_hl)
                        (void) leaf_pop(R_HL, 1);   /* dropped: as it is */
                    leaf_top_in_hl = leaf_top_in_a = 0;
                    leaf_depth--;
                }
        }
        return;
    case GL_vpush_const: case GL_vpush_bss:
        return;                         /* made again where read */
    case GL_gen_data: {
        /* A string's bytes, jumped over, as the first pass wrote them; its
         * address, a constant, read through moved_at from here on. */
        int bytes = (int) insn->rec->arg[1], put = gen_data(gl_kept(insn->rec->arg[0]), bytes);

        moved_add((int) insn->rec->ret, bytes, put);
        return;
    }
    case GL_vpush_global_addr:
        if (insn->res >= 0 && vals[insn->res].reg == HOME_GLOBAL)
            return;                     /* loaded where it is read */
        ld_rr_imm(R_HL, 0);
        fixup_add((int) insn->rec->arg[0], out_here() - ACC_INT_SIZE);
        leaf_result(insn->res);
        return;
    case I_SET: case I_CONV: {
        Type to = op == I_SET ? vals[insn->target].type : insn->local_type;

        if (leaf_keep_a(insn, to, op == I_SET ? insn->target : insn->res))
            return;
        leaf_operand_hl(insn, 0);
        leaf_convert(&insn->in[0], to);
        leaf_result(op == I_SET ? insn->target : insn->res);
        return;
    }
    case GL_vconvert: case GL_vcast:
        if (leaf_keep_a(insn, (Type) insn->rec->arg[0], insn->res))
            return;
        leaf_operand_hl(insn, 0);
        leaf_convert(&insn->in[0], (Type) insn->rec->arg[0]);
        leaf_result(insn->res);
        return;
    case I_STEP:
        if (insn->in[0].val >= 0 && leaf_in_iy(&insn->in[0]) && insn->kills & 1
            && leaf_in_iy(&(Ent) { insn->res, { 0 }, 0 })
            && type_size(insn->local_type) == ACC_INT_SIZE
            && (!type_pointer(insn->local_type)
                || type_step(insn->local_type, insn->in[0].attr.ext) == 1)) {
            step_reg(-1, insn->step_op == TK_MINUS ? -1 : 1);   /* inc iy */
            cache_write(insn, -1);
            return;
        }
        if (vals[insn->res].reg == R_BC && insn->in[0].val >= 0
            && vals[insn->in[0].val].reg == R_BC && insn->kills & 1
            && type_size(insn->local_type) == ACC_INT_SIZE
            && !type_pointer(insn->local_type)) {
            step_reg(R_BC, insn->step_op == TK_MINUS ? -1 : 1);
            cache_write(insn, R_BC);
            return;
        }
        leaf_operand_hl(insn, 0);
        number = type_pointer(insn->local_type)
                 ? type_step(insn->local_type, insn->in[0].attr.ext) : 1;
        if (number > 4)
            ld_rr_imm(R_DE, insn->step_op == TK_MINUS ? -number : number), add_hl_rr(R_DE);
        else
            step_reg(R_HL, insn->step_op == TK_MINUS ? -number : number);
        leaf_narrow(insn->local_type);
        cache_write(insn, R_HL);
        leaf_result(insn->res);
        return;
    case GL_vneg: case GL_vnot:
        leaf_operand_hl(insn, 0);
        ex_de_hl();
        if (op == GL_vneg)
            or_a_a();
        else
            out_byte(0x37);             /* scf: -1 - x is ~x */
        sbc_hl_hl();
        or_a_a();
        sbc_hl_rr(R_DE);
        leaf_result(insn->res);
        return;
    case GL_vtruth:
        if (leaf_known_width(&insn->in[0]) == 1 && leaf_byte_ok(&insn->in[0])) {
            (void) leaf_byte_to_a(&insn->in[0]);
            or_a_a();                   /* Z when the byte is 0 */
        } else {
            leaf_operand_hl(insn, 0);
            add_hl_rr(R_BC);            /* Z when HL is 0, BC as it was */
            or_a_a();
            sbc_hl_rr(R_BC);
        }
        cc = (int) insn->rec->arg[0] == TK_EQ ? JP_Z : JP_NZ;
        if (leaf_branch(cc, blk, at))
            return;
        leaf_truth_value(cc);
        leaf_result(insn->res);
        return;
    case GL_vderef: {
        /* What is read is what the pointer points at: the value the first
         * pass had after it is already widened to an int. */
        Type read = type_deref(insn->in[0].attr.type);

        if (type_is_struct(read) || type_is_array(read)) {
            leaf_operand_hl(insn, 0);           /* the address is the value */
            leaf_result(insn->res);
            return;
        }
        if (leaf_global(&insn->in[0])) {        /* ld a, (nn) or ld hl, (nn) */
            out_byte(type_size(read) == 1 ? 0x3a : 0x2a);
            leaf_global_nn(&insn->in[0]);
            if (type_size(read) == 1) {
                leaf_result_a(insn->res, read);
            } else {
                leaf_hl_type = read;
                leaf_result(insn->res);
            }
            return;
        }
        if (type_size(read) == 1) {
            if (leaf_in_iy(&insn->in[0])) {
                out_byte3(0xfd, 0x7e, 0);       /* ld a, (iy+0) */
            } else {
                leaf_operand_hl(insn, 0);
                ld_a_hl();
            }
            leaf_result_a(insn->res, read);
            return;
        }
        if (leaf_long(read))
            read = TY_UINT;                     /* its low three bytes */
        if (leaf_in_iy(&insn->in[0]) && leaf_iy_read(insn->res, 0, insn->kills & 1))
            return;
        if (leaf_in_iy(&insn->in[0])) {
            leaf_read(read, 1, 0);
        } else {
            leaf_operand_hl(insn, 0);
            leaf_read(read, 0, 0);
        }
        leaf_result(insn->res);
        return;
    }
    case GL_vaddr_array:
        /* A local array's address, as the first pass makes it, and taken
         * off its stack: the frame is laid out there, and the array's
         * place is known only when it ends. */
        vaddr_array((int) insn->rec->arg[0], (Type) insn->rec->arg[1]);
        vdrop();
        leaf_result(insn->res);
        return;
    case GL_vaddr_local:
        lea_rr_ix(R_HL, inline_moved((int) insn->rec->arg[0]));
        leaf_result(insn->res);
        return;
    case GL_gen_switch_load:
        if (type_size((Type) insn->rec->arg[1]) == 1)
            ld_a_ix(inline_moved((int) insn->rec->arg[0]));
        else
            ld_rr_ix(R_HL, inline_moved((int) insn->rec->arg[0]));
        return;
    case GL_gen_switch_case: {
        long value = insn->rec->arg[0];
        Type type = (Type) insn->rec->arg[2];

        /* ld de, value; or a; sbc hl, de; add hl, de: HL as it was for the
         * next, Z where it was the value. A char is cp n, and has no test
         * for a value it cannot have. No edge of a case has copies to
         * make: the SSA form refuses a phi a case jumps to. */
        if (type_size(type) == 1) {
            int byte = (int) value;

            if ((((unsigned) byte + (type_unsigned(type) ? 0 : 128)) & 0xffffff) > 255)
                return;
            cp_a_imm(byte);
        } else {
            ld_rr_imm(R_DE, (int) (value & 0xffffff));
            or_a_a();
            sbc_hl_rr(R_DE);
            add_hl_rr(R_DE);
        }
        if (block_now[insn->target] >= 0)
            gen_jump_cc_to(JP_Z, block_now[insn->target]);
        else
            jump_forward(jump_op(JP_Z), insn->target);
        return;
    }
    case GL_vpush_local: {
        Type type = (Type) insn->rec->arg[1];
        int disp = inline_moved((int) insn->rec->arg[0]);

        if (type_size(type) == 1) {
            ld_a_ix(disp);                      /* a byte stays in A */
            leaf_result_a(insn->res, type);
            return;
        }
        if (!leaf_long(type) && leaf_slot_to_home(insn->res, disp, type))
            return;
        ld_rr_ix(R_HL, disp);                   /* a long's low three bytes */
        leaf_hl_type = leaf_long(type) ? TY_VOID : type;
        leaf_result(insn->res);
        return;
    }
    case GL_vstore_local: {
        Type type = (Type) insn->rec->arg[1];
        int disp = inline_moved((int) insn->rec->arg[0]);

        /* An int into a long: its three bytes, and a fourth from its sign
         * -- 0 for a value that cannot be negative. */
        if (leaf_long(type)) {
            int number;

            leaf_operand_hl(insn, 0);
            ld_ix_rr(disp, R_HL);
            if (leaf_const(&insn->in[0], &number))
                ld_a_imm(number < 0 ? 0xff : 0);
            else if (type_unsigned(insn->in[0].attr.type))
                out_byte(0xaf);                 /* xor a */
            else {
                ld_a_ix(disp + 2);
                out_byte(0x17);                 /* rla: bit 23 into carry */
                out_byte(0x9f);                 /* sbc a, a */
            }
            ld_ix_a(disp + 3);
            leaf_result(insn->res);
            return;
        }

        leaf_operand_hl(insn, 0);
        leaf_convert(&insn->in[0], type);
        if (type_size(type) == ACC_INT_SIZE)
            ld_ix_rr(disp, R_HL);
        else
            store_narrow(disp, type);
        leaf_result(insn->res);
        return;
    }
    case GL_vprefix_local: case GL_vpostfix_local: {
        /* A byte stepped where it is, inc (ix+d), and read before or after;
         * anything wider in HL, and the old value kept only if it is used. */
        Type type = (Type) insn->rec->arg[1];
        int disp = inline_moved((int) insn->rec->arg[0]);
        int count = type_pointer(type) ? type_step(type, (int) insn->rec->arg[2]) : 1;
        int post = op == GL_vpostfix_local;
        int used = insn->res >= 0 && vals[insn->res].used;

        if ((int) insn->rec->arg[3] == TK_MINUS)
            count = -count;
        if (type_size(type) == 1) {
            if (used && post)
                load_narrow_into(R_HL, disp, type);
            frame_byte(count < 0 ? 0x35 : 0x34, disp);  /* inc/dec (ix+d) */
            if (used && !post)
                load_narrow_into(R_HL, disp, type);
        } else {
            ld_rr_ix(R_HL, disp);
            if (used && post)
                push_rr(R_HL);
            if (count > 4 || count < -4) {
                ld_rr_imm(R_DE, count);
                add_hl_rr(R_DE);
            } else {
                step_reg(R_HL, count);
            }
            ld_ix_rr(disp, R_HL);
            if (used && post)
                pop_rr(R_HL);
        }
        leaf_hl_type = type;
        leaf_result(insn->res);
        return;
    }
    case GL_vprefix_indirect: case GL_vpostfix_indirect: {
        /* Through the pointer in HL: a byte stepped where it is, inc (hl);
         * an int or a pointer read into DE and HL, stepped, and written
         * back, the address in DE meanwhile. */
        Type target = type_deref(insn->in[0].attr.type);
        int count = leaf_indirect_step(target);
        int post = op == GL_vpostfix_indirect;
        int used = insn->res >= 0 && vals[insn->res].used;

        if ((int) insn->rec->arg[0] == TK_MINUS)
            count = -count;
        leaf_operand_hl(insn, 0);
        if (type_size(target) == 1) {
            if (used && post)
                ld_a_hl();
            out_byte(count < 0 ? 0x35 : 0x34);          /* inc/dec (hl) */
            if (used && !post)
                ld_a_hl();
            if (used) {
                if (type_unsigned(target)) {
                    or_a_a();
                    sbc_hl_hl();
                } else {
                    ld_l_a();
                    out_byte2(0xcb, 0x05);              /* rlc l */
                    sbc_hl_hl();
                }
                ld_l_a();
            }
        } else {
            out_byte2(0xed, 0x17);                      /* ld de, (hl) */
            ex_de_hl();                                 /* old, address */
            if (used && post)
                push_rr(R_HL);
            step_reg(R_HL, count);
            ex_de_hl();
            out_byte2(0xed, 0x1f);                      /* ld (hl), de */
            if (used && post)
                pop_rr(R_HL);
            else if (used)
                ex_de_hl();
        }
        leaf_hl_type = target;
        leaf_result(insn->res);
        return;
    }
    case GL_vmember:
        /* A member of a global, left for its one use: the global's address
         * with the offset added, loaded -- or read, or written -- there. */
        if (insn->in[0].val >= 0 && vals[insn->in[0].val].reg == HOME_GLOBAL
            && insn->res >= 0 && vals[insn->res].fwd) {
            vals[insn->res].fwd = 0;
            vals[insn->res].reg = HOME_GLOBAL;
            vals[insn->res].slot = vals[insn->in[0].val].slot;
            leaf_global_off[insn->res] = leaf_global_off[insn->in[0].val]
                                         + (int) insn->rec->arg[0];
            return;
        }
        /* The member's address: the pointer and its offset. The read or
         * write is the vderef or vstore_indirect after it. */
        number = (int) insn->rec->arg[0];
        if (leaf_in_iy(&insn->in[0]) && disp_fits(number)) {
            int next = at + 1;

            /* Read straight away: ld a, (iy+d) or ld hl, (iy+d), the
             * address never made. */
            while (next < ninsns && insns[next].op == GL_vdrop && insns[next].nin == 0)
                next++;
            if (next < ninsns && insns[next].op == GL_vderef
                && insns[next].in[0].val == insn->res && insn->res >= 0
                && vals[insn->res].fwd && !insns[next].delegated
                && !(insns[next].wide && !disp_fits(number + 3))) {
                Type read = type_deref(insns[next].in[0].attr.type);

                if (insns[next].wide && disp_fits(number + 3)) {
                    int to = vals[insns[next].res].slot;   /* all four bytes */

                    if (vals[insns[next].res].used) {
                        out_byte3(0xfd, 0x27, number);  /* ld hl, (iy+d) */
                        ld_ix_rr(to, R_HL);
                        out_byte3(0xfd, 0x7e, number + 3);  /* ld a, (iy+d) */
                        ld_ix_a(to + 3);
                    }
                    leaf_skip_until = next;
                    return;
                }
                if (leaf_long(read))
                    read = TY_UINT;                     /* its low three bytes */
                if (type_is_struct(read) || type_is_array(read)) {
                    lea_rr_iy(R_HL, number);            /* its address */
                    leaf_result(insns[next].res);
                } else if (type_size(read) == 1) {
                    out_byte3(0xfd, 0x7e, number);      /* ld a, (iy+d) */
                    leaf_result_a(insns[next].res, read);
                } else if (!leaf_iy_read(insns[next].res, number, insn->kills & 1)) {
                    leaf_read(read, 1, number);
                    leaf_result(insns[next].res);
                }
                leaf_skip_until = next;
                return;
            }
            /* A constant written there straight away: ld (iy+d), n, or
             * ld hl, n / ld (iy+d), hl -- the address never made. */
            if (next < ninsns && insns[next].op == GL_vstore_indirect
                && insns[next].in[0].val == insn->res && insn->res >= 0
                && vals[insn->res].fwd && !insns[next].delegated
                && leaf_const(&insns[next].in[1], &value)) {
                Type to = type_deref(insns[next].in[0].attr.type);

                if (to == TY_BOOL)
                    value = value != 0;
                if (type_size(to) == 1) {
                    out_iy_d(0x36, number);             /* ld (iy+d), n */
                    out_byte(value & 0xff);
                    if (insns[next].res >= 0 && vals[insns[next].res].used) {
                        ld_rr_imm(R_HL, value);
                        leaf_result(insns[next].res);
                    }
                    leaf_skip_until = next;
                    return;
                }
                if (type_size(to) == ACC_INT_SIZE && !type_is_struct(to)) {
                    ld_rr_imm(R_HL, value);
                    out_iy_d(0x2f, number);             /* ld (iy+d), hl */
                    leaf_result(insns[next].res);
                    leaf_skip_until = next;
                    return;
                }
                /* A long: its low three bytes from HL, its top one after. */
                if (leaf_long(to) && disp_fits(number + 3)) {
                    value = (int) leaf_wide_bits(&insns[next].in[1]);
                    ld_rr_imm(R_HL, value);
                    out_iy_d(0x2f, number);             /* ld (iy+d), hl */
                    out_iy_d(0x36, number + 3);         /* ld (iy+d+3), n */
                    out_byte((int) ((unsigned) value >> 24));
                    leaf_result(insns[next].res);
                    leaf_skip_until = next;
                    return;
                }
            }
            lea_rr_iy(R_HL, number);
        } else {
            leaf_operand_hl(insn, 0);
            if (number > 0 && number <= 4)
                step_reg(R_HL, number);
            else if (number) {
                ld_rr_imm(R_DE, number);
                add_hl_rr(R_DE);
            }
        }
        leaf_result(insn->res);
        return;
    case GL_vstore_indirect: {
        Type to = type_deref(insn->in[0].attr.type);

        /* A struct: its bytes copied, ldir from the value's address to
         * the pointer's, BC kept; the copy's address the answer. */
        if (type_is_struct(to)) {
            int bytes = ext_bytes(insn->in[1].attr.ext);

            (void) leaf_operands_in(insn, 0);   /* where to, from */
            ex_de_hl();
            if (bytes) {
                push_rr(R_BC);
                push_rr(R_DE);
                ld_rr_imm(R_BC, bytes);
                out_byte2(0xed, 0xb0);          /* ldir */
                pop_rr(R_HL);
                pop_rr(R_BC);
            } else {
                ex_de_hl();
            }
            leaf_result(insn->res);
            return;
        }

        /* A long, which leaf_ok lets through only as a constant: the low
         * three bytes, ld (hl), de, and the top one. HL is the address
         * three on; the answer, if it is read, its low three bytes. */
        if (leaf_long(to) && leaf_const(&insn->in[1], &number)) {
            number = (int) leaf_wide_bits(&insn->in[1]);
            leaf_operand_hl(insn, 0);
            ld_rr_imm(R_DE, number);
            out_byte2(0xed, 0x1f);              /* ld (hl), de */
            step_reg(R_HL, 3);
            out_byte2(0x36, (int) ((unsigned) number >> 24));  /* ld (hl), n */
            if (insn->res >= 0 && vals[insn->res].used) {
                ld_rr_imm(R_HL, number);
                leaf_result(insn->res);
            }
            return;
        }
        if (leaf_const(&insn->in[1], &number) && type_size(to) == 1) {
            if (to == TY_BOOL)
                number = number != 0;
            leaf_operand_hl(insn, 0);
            out_byte2(0x36, number & 0xff);     /* ld (hl), n */
            if (insn->res >= 0 && vals[insn->res].used) {
                ld_rr_imm(R_HL, number);
                leaf_result(insn->res);
            }
            return;
        }
        /* To a global's member, (nn): an int from HL, a byte from A. */
        if (leaf_global(&insn->in[0]) && insn->in[1].val >= 0 && to != TY_BOOL
            && ((type_size(to) == 1 && leaf_byte_ok(&insn->in[1]))
                || type_size(to) == ACC_INT_SIZE)) {
            if (type_size(to) == 1) {
                (void) leaf_byte_to_a(&insn->in[1]);
                out_byte(0x32);                 /* ld (nn), a */
                leaf_global_nn(&insn->in[0]);
                leaf_result_a(insn->res, to);
            } else {
                leaf_operand_hl(insn, 1);
                if (!leaf_fits(leaf_held(&insn->in[1]), to))
                    leaf_narrow(to);
                out_byte(0x22);                 /* ld (nn), hl */
                leaf_global_nn(&insn->in[0]);
                leaf_result(insn->res);
            }
            return;
        }
        /* A byte in A, written where the address says. */
        if (type_size(to) == 1 && to != TY_BOOL && leaf_top_in_a
            && insn->in[1].val >= 0 && vals[insn->in[1].val].fwd) {
            leaf_depth--;
            leaf_top_in_hl = leaf_top_in_a = 0;
            if (insn->in[0].val >= 0 && vals[insn->in[0].val].fwd) {
                (void) leaf_pop(R_HL, 1);       /* an address: A kept */
                leaf_depth--;
            } else {
                leaf_load(&insn->in[0], R_HL);  /* not A: an address */
            }
            out_byte(0x77);                     /* ld (hl), a */
            leaf_result_a(insn->res, to);
            return;
        }
        number = leaf_operands_in(insn, 1);

        /* A _Bool written with a value that may be other than 0 or 1: the
         * value tested, BC and DE as they were, and 0 written and stepped
         * to 1 where it is not 0. */
        if (to == TY_BOOL && !leaf_is_01(&insn->in[1])) {
            if (number == R_BC) {
                push_rr(R_HL);
                or_a_a();
                sbc_hl_hl();
                sbc_hl_rr(R_BC);                /* Z when BC is 0 */
                pop_rr(R_HL);
            } else {
                ex_de_hl();
                add_hl_rr(R_BC);
                or_a_a();
                sbc_hl_rr(R_BC);                /* Z when DE is 0 */
                ex_de_hl();
            }
            out_byte2(0x36, 0);                 /* ld (hl), 0 */
            out_byte2(0x28, 1);                 /* jr z, past */
            out_byte(0x34);                     /* inc (hl) */
            if (insn->res >= 0 && vals[insn->res].used) {
                ld_a_hl();
                or_a_a();
                sbc_hl_hl();
                ld_l_a();
                leaf_hl_type = TY_BOOL;
                leaf_result(insn->res);
            }
            return;
        }
        if (type_size(to) == 1)
            out_byte(number == R_BC ? 0x71 : 0x73);     /* ld (hl), c / e */
        else if (number == R_BC)
            out_byte2(0xed, 0x0f);                      /* ld (hl), bc */
        else
            ld_ind_hl_de();
        if (insn->res >= 0 && vals[insn->res].used) {
            if (number == R_BC) {
                push_rr(R_BC);
                pop_rr(R_DE);
            }
            ex_de_hl();
            leaf_narrow(to);
            leaf_result(insn->res);
        }
        return;
    }
    case GL_vapply:
        number = (int) insn->rec->arg[0];
        if (insn->rec->arg[1]) {
            leaf_narrow_insn(insn, number, (Type) insn->rec->arg[1]);
            return;
        }
        switch (number) {
        case TK_LT: case TK_GT: case TK_LE: case TK_GE: case TK_EQ: case TK_NE:
            cc = leaf_compare(insn, number);
            if (leaf_branch(cc, blk, at))
                return;
            leaf_truth_value(cc);
            leaf_result(insn->res);
            return;
        case TK_PLUS: case TK_MINUS: {
            Type ltype = insn->in[0].attr.type, rtype = insn->in[1].attr.type;
            int step = 1, scaled = -1, right;

            if (type_pointer(ltype) && type_pointer(rtype)) {
                step = type_step(ltype, insn->in[0].attr.ext);
                if (step != 1) {
                    leaf_wide_step(insn, number, step, -1);
                    return;
                }
            } else if (type_pointer(ltype)) {
                step = type_step(ltype, insn->in[0].attr.ext), scaled = 1;
            } else if (type_pointer(rtype)) {
                step = type_step(rtype, insn->in[1].attr.ext), scaled = 0;
            }
            if (step < 1) {
                fail = "a step the code here does not make";
                return;
            }
            if (step > 4) {
                leaf_wide_step(insn, number, step, scaled);
                return;
            }

            /* An unsigned byte in A on top, added to or taken from what is
             * under it or loaded -- a table indexed by a byte: the byte
             * zero-extended into DE, A kept while the left comes to HL. */
            if (step == 1 && scaled != 0 && leaf_top_in_a
                && insn->in[1].val >= 0 && vals[insn->in[1].val].fwd
                && (type_unsigned(leaf_a_type) || leaf_known_width(&insn->in[1]) == 1)
                && !(insn->in[0].val >= 0 && vals[insn->in[0].val].fwd
                     && leaf_nstacked && leaf_stacked_af[leaf_nstacked - 1])
                && (insn->in[0].val == S_CONST
                    || (insn->in[0].val >= 0
                        && (vals[insn->in[0].val].fwd
                            || type_size(vals[insn->in[0].val].type) == ACC_INT_SIZE)))) {
                /* (a narrow left, loaded, is widened through A) */
                leaf_depth--;
                leaf_top_in_hl = leaf_top_in_a = 0;
                if (insn->in[0].val >= 0 && vals[insn->in[0].val].fwd) {
                    (void) leaf_pop(R_HL, 1);   /* the left: A kept */
                    leaf_depth--;
                } else {
                    leaf_load(&insn->in[0], R_HL);
                }
                ld_rr_imm(R_DE, 0);
                out_byte(0x5f);                 /* ld e, a */
                if (number == TK_PLUS) {
                    add_hl_rr(R_DE);
                } else {
                    or_a_a();
                    sbc_hl_rr(R_DE);
                }
                leaf_result(insn->res);
                return;
            }
            /* A pointer and an int in BC: the pointer in HL, and BC added
             * -- or taken -- as many times as the step. `a[j]` with j in
             * BC is lea hl, iy+0 / add hl, bc three times. */
            if (scaled >= 0 && insn->in[scaled].val >= 0
                && !vals[insn->in[scaled].val].fwd
                && vals[insn->in[scaled].val].reg == R_BC
                && (number == TK_PLUS || scaled == 1)) {
                int times;

                leaf_operand_hl(insn, 1 - scaled);      /* the pointer */
                for (times = 0; times != step; times++) {
                    if (number == TK_PLUS) {
                        add_hl_rr(R_BC);
                    } else {
                        or_a_a();
                        sbc_hl_rr(R_BC);
                    }
                }
                leaf_result(insn->res);
                return;
            }
            /* The pointer in BC and the int anywhere: the int into HL and
             * scaled there -- three times through DE -- and BC added, or
             * the scaled int taken from it. */
            if (scaled >= 0 && step > 1 && insn->in[1 - scaled].val >= 0
                && !vals[insn->in[1 - scaled].val].fwd
                && vals[insn->in[1 - scaled].val].reg == R_BC
                && (number == TK_PLUS || scaled == 1)) {
                leaf_operand_hl(insn, scaled);          /* the int */
                if (step == 3) {
                    push_rr(R_HL);
                    pop_rr(R_DE);
                }
                add_hl_hl();
                if (step == 3)
                    add_hl_rr(R_DE);
                if (step == 4)
                    add_hl_hl();
                if (number == TK_PLUS) {
                    add_hl_rr(R_BC);
                } else {
                    ex_de_hl();
                    push_rr(R_BC);
                    pop_rr(R_HL);
                    or_a_a();
                    sbc_hl_rr(R_DE);
                }
                leaf_result(insn->res);
                return;
            }
            right = leaf_operands_in(insn, step == 1);

            /* Three bytes: the int in DE added to the pointer, or taken
             * from it, three times -- no copy of either kept on the stack
             * while the int is scaled. */
            if (step == 3) {
                int times;

                if (scaled == 0)
                    ex_de_hl();         /* the pointer into HL */
                for (times = 0; times != 3; times++) {
                    if (number == TK_PLUS) {
                        add_hl_rr(R_DE);
                    } else {
                        or_a_a();
                        sbc_hl_rr(R_DE);
                    }
                }
                leaf_result(insn->res);
                return;
            }
            if (scaled == 1 && step != 1)
                ex_de_hl();             /* the int, on the right, into HL */
            if (step == 3) {
                push_rr(R_DE);          /* the pointer */
                push_rr(R_HL);
                add_hl_hl();
                pop_rr(R_DE);
                add_hl_rr(R_DE);        /* the int three times */
                pop_rr(R_DE);
            }
            if (step == 2 || step == 4)
                add_hl_hl();
            if (step == 4)
                add_hl_hl();
            if (scaled == 1 && step != 1)
                ex_de_hl();             /* and the pointer back */
            if (number == TK_PLUS) {
                add_hl_rr(right);
            } else {
                or_a_a();
                sbc_hl_rr(right);
            }
            leaf_result(insn->res);
            return;
        }
        case TK_SHL: {
            int count;

            if (!leaf_const(&insn->in[1], &count) || count < 0 || count > 8) {
                leaf_helper(insn, RT_SHL);
                return;
            }
            leaf_operand_hl(insn, 0);
            while (count--)
                add_hl_hl();
            leaf_result(insn->res);
            return;
        }
        case TK_SHR: {
            int count;

            /* An unsigned byte shifted by a little: srl a, in A. */
            if (leaf_const(&insn->in[1], &count) && count >= 0 && count <= 8
                && leaf_known_width(&insn->in[0]) == 1 && leaf_byte_ok(&insn->in[0])) {
                (void) leaf_byte_to_a(&insn->in[0]);
                while (count--)
                    out_byte2(0xcb, 0x3f);      /* srl a */
                leaf_hl_width = 1;
                leaf_result_a(insn->res, TY_UCHAR);
                return;
            }
            leaf_helper(insn, type_unsigned(type_promote(insn->in[0].attr.type))
                              ? RT_SHRU : RT_SHRS);
            return;
        }
        case TK_SLASH: case TK_PERCENT: {
            int is_unsigned = type_unsigned(insn->in[0].attr.type)
                              || type_unsigned(insn->in[1].attr.type);

            leaf_helper(insn, number == TK_SLASH ? (is_unsigned ? RT_DIVU : RT_DIVS)
                                                 : (is_unsigned ? RT_REMU : RT_REMS));
            return;
        }
        case TK_STAR: {
            int which = leaf_const(&insn->in[1], &(int) { 0 }) ? 1
                        : leaf_const(&insn->in[0], &(int) { 0 }) ? 0 : -1, by;

            if (which >= 0 && leaf_const(&insn->in[which], &by) && leaf_mul_ok(by)) {
                leaf_operand_hl(insn, 1 - which);
                leaf_mul(by);
                leaf_result(insn->res);
                return;
            }
            leaf_helper(insn, RT_MUL);
            return;
        }
        case TK_AMP: case TK_PIPE: case TK_CARET: {
            int byte_op = number == TK_AMP ? 0xa3 : number == TK_PIPE ? 0xb3 : 0xab;

            if (number != TK_AMP && (leaf_byte_const(&insn->in[0])
                                     || leaf_byte_const(&insn->in[1]))) {
                int which = leaf_byte_const(&insn->in[1]) ? 1 : 0, byte;

                (void) leaf_const(&insn->in[which], &byte);
                if (leaf_known_width(&insn->in[1 - which]) == 1
                    && leaf_byte_ok(&insn->in[1 - which])) {
                    (void) leaf_byte_to_a(&insn->in[1 - which]);
                    out_byte2(number == TK_PIPE ? 0xf6 : 0xee, byte);   /* or n, xor n */
                    leaf_result_a(insn->res, TY_UCHAR);
                    return;
                }
                leaf_hl_width = (unsigned char) leaf_known_width(&insn->in[1 - which]);
                leaf_operand_hl(insn, 1 - which);
                ld_a_l();
                out_byte2(number == TK_PIPE ? 0xf6 : 0xee, byte);   /* or n, xor n */
                ld_l_a();
                leaf_result(insn->res);
                return;
            }
            if (number == TK_AMP && (leaf_byte_const(&insn->in[0])
                                     || leaf_byte_const(&insn->in[1]))) {
                int which = leaf_byte_const(&insn->in[1]) ? 1 : 0, byte;

                (void) leaf_const(&insn->in[which], &byte);
                if (!leaf_byte_to_a(&insn->in[1 - which])) {
                    leaf_operand_hl(insn, 1 - which);
                    ld_a_l();
                }
                out_byte2(0xe6, byte);                  /* and n */
                if (leaf_and_branch(insn, blk, at))
                    return;
                leaf_result_a(insn->res, TY_UCHAR);
                return;
            }
            if (leaf_bitwise(insn, number, byte_op) || leaf_byte_op(insn, number))
                return;
            if (leaf_operands_in(insn, 1) == R_BC)
                byte_op -= 2;           /* and c, or c, xor c */
            ld_a_l();
            out_byte(byte_op);          /* and e, or e, xor e */
            if (number == TK_AMP)
                sbc_hl_hl();            /* and leaves carry clear */
            ld_l_a();
            leaf_hl_width = 1;
            leaf_result(insn->res);
            return;
        }
        }
        fail = "internal: an operator leaf_ok let through";
        return;
    case GL_gen_call: {
        const Sym *callee = sym_at((int) insn->rec->arg[0]);
        int fn = (int) insn->rec->arg[0], first = (int) insn->rec->arg[2];
        int nparams = (int) insn->rec->arg[3], arg, homes, waiting = 0;
        int nslots = 0, wide_answer = leaf_long(callee->type);

        /* The arguments pushed last first, each as its parameter's type,
         * so the first is at the lowest address -- as agondev passes them.
         * Only the last may be left on the stack, in HL. */
        leaf_widen_a();

        /* IY and BC, where a value in either lives across the call, kept
         * on the stack under the arguments -- push iy, push bc: a byte or
         * two each way -- unless an argument waits on the machine stack,
         * which that would bury: then in their slots, after them. */
        homes = leaf_live_homes(at, blk);
        for (arg = 0; arg != insn->nin; arg++)
            if (insn->in[arg].val >= 0 && vals[insn->in[arg].val].fwd)
                waiting++;
        if (waiting > 1 || (waiting && !leaf_top_in_hl))
            waiting = -1;               /* on the machine stack: slots */
        if (waiting >= 0) {
            if (homes & 1)
                out_byte2(0xfd, 0xe5);  /* push iy */
            if (homes & 2)
                push_rr(R_BC);
        }
        for (arg = insn->nin - 1; arg >= 0; arg--) {
            const Ent *ent = &insn->in[arg];
            int waiting = 0, before;

            for (before = 0; before != arg; before++)
                if (insn->in[before].val >= 0 && vals[insn->in[before].val].fwd)
                    waiting = 1;
            if (leaf_long_arg(insn, arg)) {
                /* Two slots: the high byte's pushed first, then the low
                 * three -- through DE where one waits in HL. */
                int reg = waiting ? R_DE : R_HL;

                if (waiting)
                    leaf_top_to_hl();
                if (ent->val == S_CONST) {
                    ld_rr_imm(reg, (int) (leaf_wide_bits(ent) >> 24));
                    push_rr(reg);
                    ld_rr_imm(reg, (int) (leaf_wide_bits(ent) & 0xffffff));
                } else {
                    ld_rr_ix(reg, vals[ent->val].slot + 3);
                    push_rr(reg);
                    ld_rr_ix(reg, vals[ent->val].slot);
                }
                push_rr(reg);
                nslots += 2;
                continue;
            }
            nslots++;
            if (ent->val >= 0 && vals[ent->val].fwd) {
                leaf_top_to_hl();
                leaf_depth--;
                leaf_top_in_hl = 0;
            } else if (waiting) {
                /* One before it waits on the stack: that one into HL, and
                 * this one around it, through DE, as it is --
                 * leaf_early_argument saw to that. Pushed with the waiting
                 * one still on the machine stack, it would bury it. */
                leaf_top_to_hl();
                leaf_load(ent, R_DE);
                push_rr(R_DE);
                continue;
            } else {
                leaf_load(ent, R_HL);
            }
            if (arg < nparams)
                leaf_convert(ent, sym_param_type(first, arg));
            push_rr(R_HL);
        }
        if (waiting < 0)
            leaf_keep_homes(homes, 0);
        if (sym_flags(fn) & SYMF_DEFINED) {
            want(fn);
            out_reloc(out_here() + 1);
            out_opcode24(0xcd, callee->val);            /* call nn */
        } else {
            out_opcode24(0xcd, 0);
            fixup_add(fn, out_here() - ACC_INT_SIZE);
        }
        if (wide_answer && nslots)
            out_byte(0x7b);                     /* ld a, e: the high byte */
        for (arg = 0; arg != nslots; arg++)
            pop_rr(R_DE);
        if (waiting < 0) {
            leaf_keep_homes(homes, 1);
        } else {
            if (homes & 2)
                pop_rr(R_BC);
            if (homes & 1)
                out_byte2(0xfd, 0xe1);  /* pop iy */
        }

        /* A long's answer is in E:HL -- its high byte in A by now, where
         * arguments were taken off through DE -- and goes to its slot. */
        if (wide_answer) {
            if (insn->res < 0 || !vals[insn->res].used)
                return;
            if (!leaf_wide_val(insn->res)) {    /* held as its low bytes */
                leaf_hl_type = TY_ULONG;
                leaf_result(insn->res);
                return;
            }
            ld_ix_rr(vals[insn->res].slot, R_HL);
            if (nslots)
                ld_ix_a(vals[insn->res].slot + 3);
            else
                frame_byte(0x73, vals[insn->res].slot + 3);    /* ld (ix+d), e */
            return;
        }
        /* A byte's answer is in A, and stays there; the rest in HL. */
        if (callee->type != TY_VOID && type_size(callee->type) == 1) {
            leaf_result_a(insn->res, callee->type == TY_BOOL ? TY_UCHAR : callee->type);
            return;
        }
        leaf_hl_type = callee->type;
        leaf_result(insn->res);
        return;
    }
    case GL_gen_return:
        /* A constant answer goes to gen_return as the constant, which
         * knows it -- a return of one made before is a jump back to it --
         * and makes a _Bool of it as it is; anything else in HL. */
        if (insn->nin && insn->in[0].val == S_CONST) {
            load_constant(&insn->in[0]);
        } else if (insn->nin) {
            leaf_operand_hl(insn, 0);
            vpush(VAL_REG, insn->in[0].attr.type, R_HL);
        }
        if (leaf_depth)
            fail = "internal: values left under a return";
        call(insn);
        return;
    case I_JMP:
        if (insn->target < 0)
            return;
        leaf_edge(blk, insn->target);
        if (block_now[insn->target] >= 0)
            gen_jump_to(block_now[insn->target]);
        else if (!falls_to(blk, insn->target) || block_last[blk] != at)
            jump_forward(gen_jump(), insn->target);
        else
            leaf_jump_left_out = 1;     /* falls into it: see emit_leaf */
        return;
    case I_BR:
        if (at == skip_branch)
            return;
        if (insn->target < 0) {
            if (insn->nin)
                leaf_operand_hl(insn, 0);
            return;
        }
        /* On a constant: never taken, nothing -- a macro's do ... while
         * (0) -- or always, a jump with the edge's copies. */
        if (leaf_const(&insn->in[0], &number)) {
            if ((number != 0) != insn->sense)
                return;
            leaf_edge(blk, insn->target);
            if (block_now[insn->target] >= 0)
                gen_jump_to(block_now[insn->target]);
            else
                jump_forward(gen_jump(), insn->target);
            return;
        }
        if (leaf_known_width(&insn->in[0]) == 1 && leaf_byte_ok(&insn->in[0])) {
            (void) leaf_byte_to_a(&insn->in[0]);
            or_a_a();
        } else {
            leaf_operand_hl(insn, 0);
            add_hl_rr(R_BC);
            or_a_a();
            sbc_hl_rr(R_BC);
        }
        branch_to(JP_NZ, insn, blk);
        return;
    }
    fail = "internal: an instruction leaf_ok let through";
}

/* Whether `blk`'s end falls into `target`: it is the next block, or only
 * blocks nothing reaches lie between, which are not made. */
static int falls_to(int blk, int target)
{
    int between;

    if (target <= blk)
        return 0;
    for (between = blk + 1; between != target; between++)
        if (rpo_num[between] >= 0)
            return 0;

    return 1;
}

/* The copies an edge makes into its target's phis: every value the edge
 * brings pushed first, then each popped to its phi's home -- the last
 * first -- so that none is written before all are read. */
static void leaf_edge(int from, int to)
{
    int dests[MAX_LOCALS * 4], sources[MAX_LOCALS * 4], nmoves = 0, pred, phi;

    pred = pred_index(to, from);
    if (pred >= 0) {
        for (phi = phi_head[to]; phi >= 0; phi = phi_next[phi]) {
            const Phi *join = &phis[phi];
            int source;

            source = join->in[pred];
            if (source < 0 || source == join->val || !vals[join->val].used
                || (vals[source].reg == vals[join->val].reg
                    && (vals[source].reg != HOME_SLOT
                        || vals[source].slot == vals[join->val].slot))
                || nmoves == (int) (sizeof dests / sizeof dests[0]))
                continue;
            sources[nmoves] = source;
            dests[nmoves++] = join->val;
        }
    }

    /* One copy is made straight, through HL; more go through the stack,
     * all read before any is written, since one may be another's source. */
    for (phi = 0; phi != nmoves; phi++)
        if (leaf_wide_val(dests[phi]) && !leaf_wide_val(sources[phi])) {
            fail = "internal: a long's phi from what is not one";
            return;
        }
    if (nmoves == 1 && leaf_wide_val(dests[0])) {
        Ent ent;

        memset(&ent, 0, sizeof ent);
        ent.val = sources[0];
        leaf_wide_copy(&ent, vals[dests[0]].slot);
        return;
    }
    if (nmoves == 1) {
        Ent ent;

        memset(&ent, 0, sizeof ent);
        ent.val = sources[0];
        ent.attr.type = vals[sources[0]].type;
        vals[dests[0]].fwd = 0;
        if (vals[sources[0]].reg == HOME_SLOT
            && !(iy_web >= 0 && vals[sources[0]].slot == fixed_slot[iy_web])
            && type_size(vals[sources[0]].type) == ACC_INT_SIZE
            && leaf_slot_to_home(dests[0], vals[sources[0]].slot,
                                 vals[sources[0]].type))
            return;
        leaf_load(&ent, R_HL);
        leaf_result(dests[0]);
        return;
    }
    for (phi = 0; phi != nmoves; phi++) {
        Ent ent;

        memset(&ent, 0, sizeof ent);
        ent.val = sources[phi];
        ent.attr.type = vals[sources[phi]].type;
        if (leaf_wide_val(dests[phi])) {      /* all four bytes */
            ld_rr_ix(R_HL, vals[sources[phi]].slot);
            push_rr(R_HL);
            ld_a_ix(vals[sources[phi]].slot + 3);
            out_byte(0xf5);                     /* push af */
        } else if (vals[sources[phi]].reg == R_BC) {
            push_rr(R_BC);
        } else {
            leaf_load(&ent, R_HL);
            push_rr(R_HL);
        }
    }
    while (nmoves--) {
        int dest = dests[nmoves];

        if (leaf_wide_val(dest)) {
            out_byte(0xf1);                     /* pop af */
            ld_ix_a(vals[dest].slot + 3);
            pop_rr(R_HL);
            ld_ix_rr(vals[dest].slot, R_HL);
        } else if (vals[dest].reg == R_BC) {
            pop_rr(R_BC);
        } else {
            pop_rr(R_HL);
            vals[dest].fwd = 0;
            leaf_result(dest);
        }
    }
}

static void emit_leaf(void)
{
    int at, blk, local, *block_start;

    block_now = malloc((size_t) nblocks * sizeof *block_now);
    block_start = malloc(((size_t) nblocks + 1) * sizeof *block_start);
    if (!block_now || !block_start)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != nblocks; at++)
        block_now[at] = block_start[at] = -1;
    emit_start();
    nmoved = 0;
    ntrampolines = 0;
    skip_branch = fused_from = leaf_skip_until = -1;
    leaf_depth = leaf_nstacked = 0;
    leaf_top_in_hl = leaf_top_in_a = 0;
    leaf_hl_type = TY_VOID;
    leaf_hl_width = 3;
    leaf_holds = realloc(leaf_holds, ((size_t) nvals + 1) * sizeof *leaf_holds);
    leaf_widths = realloc(leaf_widths, (size_t) nvals + 1);
    leaf_global_off = realloc(leaf_global_off, ((size_t) nvals + 1) * sizeof *leaf_global_off);
    if (!leaf_holds || !leaf_widths || !leaf_global_off)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != nvals; at++) {
        leaf_holds[at] = TY_VOID;
        leaf_widths[at] = 3;
        leaf_global_off[at] = 0;
    }

    blocks_from = out_here();
    call(&insns[0]);
    nlocal_moves = 0;
    for (at = 1; at != ninsns; at++)
        if (insns[at].op == I_FRAME && !frame_of_value(&insns[at]))
            frame_again(&insns[at]);
    gen_local_settle();                 /* the values' slots live throughout */
    leaf_save_slots();

    /* A global's address left on the stack for one use is loaded where it
     * is used instead, as a constant is: a table indexed by what is made
     * after it pushed it and popped it back. */
    for (at = 1; at != ninsns; at++)
        if (insns[at].op == GL_vpush_global_addr && insns[at].res >= 0
            && vals[insns[at].res].fwd) {
            vals[insns[at].res].fwd = 0;
            vals[insns[at].res].reg = HOME_GLOBAL;
            vals[insns[at].res].slot = (int) insns[at].rec->arg[0];
        }
    give_slots();
    inline_slots();
    for (local = 0; local != nlocals; local++)
        if (locals[local].cached)
            vals[locals[local].mem_val].slot = inline_moved(locals[local].offset);

    /* Each parameter into its first value's home, if not its own slot. */
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param) {
            int val = locals[local].entry_val;
            Type type = locals[local].type;

            if (!vals[val].used || (vals[val].reg == HOME_SLOT
                                    && vals[val].slot == locals[local].offset))
                continue;
            if (vals[val].reg == R_BC) {
                if (type_size(type) == ACC_INT_SIZE) {
                    ld_rr_ix(R_BC, locals[local].offset);
                } else if (type_unsigned(type)) {
                    load_narrow_into(R_BC, locals[local].offset, type);
                } else {
                    load_narrow_into(R_HL, locals[local].offset, type);
                    push_rr(R_HL);
                    pop_rr(R_BC);
                }
                leaf_holds[val] = type;
            } else if (iy_web >= 0 && vals[val].reg == HOME_SLOT
                       && vals[val].slot == fixed_slot[iy_web]
                       && type_size(type) == ACC_INT_SIZE) {
                frame_byte(0x31, locals[local].offset);     /* ld iy, (ix+d) */
                leaf_holds[val] = type;
                leaf_widths[val] = 3;
            } else {
                if (type_size(type) == ACC_INT_SIZE)
                    ld_rr_ix(R_HL, locals[local].offset);
                else
                    load_narrow_into(R_HL, locals[local].offset, type);
                leaf_result(val);
            }
        }

    emit_raws();
    for (blk = 0; blk != nblocks && !fail; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;
        int falls = 1;

        if (blk && rpo_num[blk] < 0)
            continue;                   /* nothing reaches it */
        if (blk)
            emit_block_start(blk);
        else
            block_now[0] = out_here();
        block_start[blk] = block_now[blk];
        leaf_jump_left_out = 0;
        for (at = blocks[blk].first; at != end && !fail; at++) {
            if (at == 0)
                continue;
            leaf_insn(&insns[at], blk, at);
            if (at == end - 1 && (insns[at].op == I_JMP
                                  || insns[at].op == GL_gen_return))
                falls = 0;
        }
        if (fail)
            break;
        if (leaf_depth || leaf_nstacked)
            fail = "internal: values left on the stack at a block's end";
        if (falls && blk + 1 < nblocks)
            leaf_edge(blk, blk + 1);

        /* A jump left out falls into the block it goes to, over blocks
         * nothing reaches, so the trampolines waiting are not made here --
         * the code would fall into the first of them. */
        if (!falls && !leaf_jump_left_out)
            emit_trampolines();
    }
    if (ntrampolines && !fail) {
        int over = gen_jump();

        emit_trampolines();
        gen_label(over);
    }
    if (!fail)
        costs(block_start, out_here());
    free(block_start);
    free(block_now);
    block_now = NULL;
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
        if (native_on() && native_step(insn, at))
            break;
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
        if (at == skip_branch)
            break;                      /* made with its comparison */
        emit_branch_regs(insn, blk, at);
        break;
    case GL_gen_switch_load:
        pins_restore();
        gen_switch_load(inline_moved((int) insn->rec->arg[0]),
                        (Type) insn->rec->arg[1]);
        pins_check();
        break;
    case GL_gen_switch_case:
        pins_restore();
        gen_switch_case(insn->rec->arg[0], (uint32_t) insn->rec->arg[1],
                        (Type) insn->rec->arg[2], block_now[insn->target],
                        inline_moved((int) insn->rec->arg[4]));
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
        if (native_on() && insn->op == GL_vapply
            && (native_compare(insn, blk, at) || native_byte_zero(insn, blk, at)
                || native_sum(insn, at)))
            break;
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

long ssa_cost_made, ssa_cost_first;
int ssa_size_made, ssa_size_first;


static long block_weight(int blk)
{
    int depth = loop_depth[blk];

    return 1L << (3 * (depth > 5 ? 5 : depth));
}

/* An eZ80 instruction at `code`, in ADL mode: its length, and in *cycles
 * about what it takes -- a cycle a byte fetched, and one a byte read or
 * written besides, which is what the chip's timing comes to for all but
 * the jumps taken. Only what acc writes needs to be right; anything else
 * is counted a byte and a cycle. */
static int ez80_insn(const unsigned char *code, int avail, int *cycles)
{
    int op = code[0], len = 1, mem = 0;

    if (op == 0xdd || op == 0xfd) {
        int next = avail > 1 ? code[1] : 0;

        if (next == 0xcb)
            len = 4, mem = 2;
        else if (next == 0x21)
            len = 5;                            /* ld ix, nn */
        else if (next == 0x22 || next == 0x2a)
            len = 5, mem = 3;                   /* ld (nn), ix */
        else if (next == 0x36)
            len = 4, mem = 1;                   /* ld (ix+d), n */
        else if (next == 0x07 || next == 0x17 || next == 0x27 || next == 0x31
                 || next == 0x37 || next == 0x0f || next == 0x1f
                 || next == 0x2f || next == 0x3e || next == 0x3f)
            len = 3, mem = 3;                   /* ld rr, (ix+d) and back */
        else if ((next >= 0x46 && next <= 0x7e && (next & 7) == 6)
                 || (next >= 0x70 && next <= 0x77) || (next & 0xc7) == 0x86
                 || next == 0x34 || next == 0x35)
            len = 3, mem = 1;                   /* a byte at (ix+d) */
        else if (next == 0xe5 || next == 0xe1)
            len = 2, mem = 3;                   /* push ix, pop ix */
        else if (next == 0xe3)
            len = 2, mem = 6;
        else
            len = 2;
    } else if (op == 0xed) {
        int next = avail > 1 ? code[1] : 0;

        if ((next & 0xcf) == 0x43 || (next & 0xcf) == 0x4b)
            len = 5, mem = 3;                   /* ld (nn), rr and back */
        else if ((next & 0xc7) == 0x02 || (next & 0xc7) == 0x03
                 || next == 0x32 || next == 0x33 || next == 0x54 || next == 0x55
                 || next == 0x64 || next == 0x65 || next == 0x74)
            len = 3;                            /* lea, and the like */
        else if (next == 0x07 || next == 0x17 || next == 0x27 || next == 0x31
                 || next == 0x37 || next == 0x0f || next == 0x1f
                 || next == 0x2f || next == 0x3e || next == 0x3f)
            len = 2, mem = 3;                   /* ld rr, (hl) and back */
        else if (next == 0xb0 || next == 0xb8)
            len = 2, mem = 2;                   /* ldir, a byte a trip */
        else
            len = 2;
    } else if (op == 0xcb) {
        len = 2;
        if ((code[1] & 7) == 6)
            mem = 2;
    } else if ((op & 0xcf) == 0x01) {
        len = 4;                                /* ld rr, nn */
    } else if (op == 0x22 || op == 0x2a) {
        len = 4, mem = 3;
    } else if (op == 0x32 || op == 0x3a) {
        len = 4, mem = 1;
    } else if ((op & 0xc7) == 0x06 || (op & 0xc7) == 0xc6 || op == 0x18
               || op == 0x10 || (op & 0xe7) == 0x20 || op == 0xd3 || op == 0xdb) {
        len = 2;                                /* n, and jr */
        if (op == 0x36)
            mem = 1;
    } else if (op == 0xc3 || (op & 0xc7) == 0xc2) {
        len = 4;                                /* jp */
    } else if (op == 0xcd || (op & 0xc7) == 0xc4) {
        len = 4, mem = 6;                       /* call, and the ret */
    } else if ((op & 0xcb) == 0xc1) {
        mem = 3;                                /* push rr, pop rr */
    } else if (op == 0xe3) {
        mem = 6;                                /* ex (sp), hl */
    } else if ((op & 0xc7) == 0x46 || (op >= 0x70 && op <= 0x77 && op != 0x76)
               || (op & 0xc7) == 0x86 || op == 0x34 || op == 0x35
               || op == 0x0a || op == 0x1a || op == 0x02 || op == 0x12) {
        mem = 1;                                /* a byte through a pointer */
    }
    if (len > avail)
        len = avail;
    *cycles = len + mem;

    return len;
}

/* The cycles of the code from `from` for `len` bytes, each instruction
 * counted by the loops around the block its first byte is in. */
static long code_cost(const unsigned char *code, int len, const int *block_of)
{
    long cost = 0;
    int at = 0;

    while (at < len) {
        int cycles, step = ez80_insn(code + at, len - at, &cycles);

        cost += block_weight(block_of[at] < 0 ? 0 : block_of[at]) * cycles;
        at += step;
    }

    return cost;
}

/* What each would cost to run: every instruction's cycles, weighted by the
 * loops around its block -- the first pass's code placed in blocks by the
 * log's records, which say where each call's bytes ended and which of
 * them are in which block. */
void costs(const int *block_start, int end)
{
    int *rec_block = malloc(((size_t) gl_n + 1) * sizeof *rec_block);
    int *byte_block, at, blk = 0, before, from = blocks_from;

    if (!rec_block)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != gl_n; at++)
        rec_block[at] = -1;
    for (at = 0; at != ninsns; at++)
        if (insns[at].rec)
            rec_block[insns[at].rec - gl_log] = insns[at].block;

    ssa_cost_first = 0;
    if (gl_first_code && gl_first_len > 0) {
        byte_block = malloc((size_t) gl_first_len * sizeof *byte_block + 1);
        if (!byte_block)
            acc_error("out of memory for the SSA form");
        for (at = 0; at != gl_first_len; at++)
            byte_block[at] = 0;
        before = gl_log[0].at_after - gl_first_from;
        for (at = 1; at != gl_n; at++) {
            int after = gl_log[at].at_after - gl_first_from, byte;

            if (rec_block[at] >= 0)
                blk = rec_block[at];
            for (byte = before < 0 ? 0 : before;
                 byte < after && byte < gl_first_len; byte++)
                byte_block[byte] = blk;
            before = after;
        }
        ssa_cost_first = code_cost(gl_first_code, gl_first_len, byte_block);
        free(byte_block);
    }
    free(rec_block);

    /* And the code made here, from where each block began. */
    byte_block = malloc(((size_t) (end - from) + 1) * sizeof *byte_block);
    if (!byte_block)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != end - from; at++)
        byte_block[at] = 0;
    for (blk = 0; blk != nblocks; blk++) {
        int next = end, later;

        if (block_start[blk] < 0)
            continue;
        for (later = blk + 1; later != nblocks; later++)
            if (block_start[later] >= 0) {
                next = block_start[later];
                break;
            }
        for (at = block_start[blk]; at < next && at < end; at++)
            byte_block[at - from] = blk;
    }
    ssa_cost_made = code_cost(out_img + (from - out_base), end - from, byte_block);
    ssa_size_made = end - from;
    ssa_size_first = gl_first_len;
    free(byte_block);
}

static void emit_regs(void)
{
    int at, blk, local, *block_start;

    block_now = malloc((size_t) nblocks * sizeof *block_now);
    if (!block_now)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != nblocks; at++)
        block_now[at] = -1;
    emit_start();
    nmoved = 0;
    ntrampolines = 0;
    npins = 0;
    skip_branch = -1;
    block_start = malloc(((size_t) nblocks + 1) * sizeof *block_start);
    if (!block_start)
        acc_error("out of memory for the SSA form");
    for (at = 0; at != nblocks; at++)
        block_start[at] = -1;

    blocks_from = out_here();
    call(&insns[0]);
    nlocal_moves = 0;
    for (at = 1; at != ninsns; at++)
        if (insns[at].op == I_FRAME && !frame_of_value(&insns[at]))
            frame_again(&insns[at]);
    gen_local_settle();                 /* the values' slots live throughout */
    give_slots();
    inline_slots();

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

    emit_raws();
    for (blk = 0; blk != nblocks && !fail; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;
        int falls = 1;

        if (blk)
            emit_block_start(blk);
        else
            block_now[0] = out_here();
        block_start[blk] = block_now[blk];
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
    if (!fail)
        costs(block_start, out_here());
    free(block_start);
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
    bset_free(&live_in);
    bset_free(&live_out);
    bset_free(&live_top);
    free(clash_off);
    free(clash_adj);
    clash_off = clash_adj = NULL;
    call_homes_n = -1;
    succs = preds = dom_kids = NULL;
    block_last = rpo = rpo_num = idom = repl = NULL;
}

int ssa_generate(const char **why)
{
    int *keep = malloc((size_t) gl_n * sizeof *keep + 1), rec_at;

    if (!keep)
        acc_error("out of memory for the SSA form");
    ninsns = nvals = nblocks = nholes = nstk = ninlined = nmerged = 0;
    holes_start();
    nraws = nsettles = 0;
    fail = NULL;
    ssa_cost_made = ssa_cost_first = 0;
    leaf_mode = ssa_made_leaf = ssa_cached_refused = ssa_cached_used = 0;
    leaf_low_said = NULL;               /* the last function's */
    lindex_ok = 0;
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
    ssa_work = 0;
    ssa_work_max = SSA_WORK_PER * ((long) ninsns + nvals + nblocks + 64);
    to_values();
    inline_merge();
    ssa_work_max = SSA_WORK_PER * ((long) ninsns + nvals + nblocks + 64);
    ssa_made_mir = 0;
    if (!fail && regs_on() && mir_on()) {
        sink_steps();
        ssa_made_mir = mir_build();
        if (!ssa_made_mir && getenv("OPTACC_SSA_STATS"))
            fprintf(stderr, "not mir %s: %s\n", name_text(sym_at(gl_fn)->name),
                    mir_reason());
        if (ssa_made_mir) {
            if (getenv("OPTACC_SSA_STATS") && mir_parts())
                fprintf(stderr, "mir %s, split into %d parts\n",
                        name_text(sym_at(gl_fn)->name), mir_parts());
            else if (getenv("OPTACC_SSA_STATS"))
                fprintf(stderr, "mir %s\n", name_text(sym_at(gl_fn)->name));
            gen_local_settle();
            mir_emit();
            forget();
            if (fail) {
                *why = fail;
                return -1;              /* part emitted: the caller stops */
            }

            return 1;
        }
    }
    if (!fail && regs_on()) {
        leaf_why = NULL;
        leaf_mode = leaf_on() && leaf_ok();
        ssa_made_leaf = leaf_mode;
        if (!leaf_mode) {               /* a wide phi only it copies */
            int phi;

            for (phi = 0; phi != nphis && !fail; phi++)
                if (phis[phi].live && type_wide(vals[phis[phi].val].type))
                    fail = wide_phi;
        }
        ssa_cached_used = cached_any;
        if (cached_any && !leaf_mode) {
            ssa_cached_refused = 1;
            fail = "a cached local, which the leaf backend alone makes";
        }
        sink_steps();
    }
    if (!fail) {
        live_ranges();
        if (regs_on()) {
            int val;

            find_forwarded();
            for (val = 0; val != nvals && !fail; val++)
                if (type_is_struct(vals[val].type) && !vals[val].fwd
                    && vals[val].used)
                    fail = "a struct as a value";   /* one never read is
                                                     * dropped, and fine */
            if (!fail)
                find_clashes();
        }
        if (!fail) {
            coalesce();
            if (regs_on())
                plan_homes();
            if (getenv("OPTACC_SSA_DUMP"))
                dump();
        }
    }
    if (fail == wide_phi && !wide_in_memory) {
        int made;

        forget();
        wide_in_memory = 1;
        made = ssa_generate(why);
        wide_in_memory = 0;

        return made;
    }
    gen_local_settle();                 /* plan_slots counts from all of it */
    if (fail || !plan_slots()) {
        *why = fail;
        forget();
        return 0;
    }
    if (leaf_mode && getenv("OPTACC_SSA_STATS"))
        fprintf(stderr, "leaf %s\n", name_text(sym_at(gl_fn)->name));
    else if (leaf_on() && regs_on() && getenv("OPTACC_SSA_STATS"))
        fprintf(stderr, "not leaf %s: %s\n", name_text(sym_at(gl_fn)->name),
                leaf_why ? leaf_why : "?");
    if (leaf_mode)
        emit_leaf();
    else if (regs_on())
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
