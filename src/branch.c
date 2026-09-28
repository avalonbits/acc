/*
 * Jumps: where they go and how far, conditions and && and ||, switch,
 * labels, and ?:.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "runtime.h"
#include "gen_int.h"

/* ------------------------------------------------------------------ */
/* branches                                                            */

/* Every jump here is a three-byte absolute `jp` and never a two-byte `jr`.
 * A relative jump would be smaller, but its offset is a signed byte, so
 * emitting one means knowing the distance to a target that has not been
 * reached yet -- which in a one-pass compiler means either guessing and
 * fixing up, or a second pass over the code. Neither is worth two bytes a
 * branch yet, and the shape here leaves room to do it later: every jump
 * already goes through one place.
 *
 * Nothing is live across a branch. The value stack is empty at every
 * statement boundary, which is where all of these are emitted, so no register
 * has to survive one and there is no state to reconcile where two paths
 * meet. */

int gen_here(void)
{
    join_at = out_here();

    return out_here();
}

/* Every jump the compiler writes, so that the ones whose target turns out to
 * be near can be written again in two bytes.
 *
 * `jr` reaches 127 bytes either way and most jumps in compiled code go no
 * further -- the end of an if, the top of a loop -- but which ones do is not
 * known when they are written: the target of a forward jump is wherever the
 * statement after it ends up. So they are all written as `jp`, and the ones
 * that turn out to be near are shortened at the end of the file, where the
 * distance is a subtraction. See relax_jumps.
 *
 * Two arrays rather than one of pairs: on this chip an index into a table is
 * a multiply by the width of an entry, and a byte's is not a multiply at
 * all. And appended through a pointer rather than at an index, for the same
 * reason: `jump_at[njumps] = ...` is that multiply, on every jump the
 * compiler writes, and the count is kept beside the pointer rather than
 * worked back out of it because undoing the multiply is a divide.
 *
 * jumps_rewind is the one way the count goes down other than one at a time,
 * and it is what keeps the two in step. */
/* Whether code is being written into a function. Jumps are recorded and
 * shortened a function at a time, so one written outside any -- there are
 * none today, and the guard in jump_op is what keeps it that way -- is
 * recorded nowhere and has to say where it is at once. */
int in_function;

int           *jump_at, *jump_put;
unsigned char *jump_cc, *jump_cc_put;
int            njumps, jumps_cap;

void jumps_rewind(int n)
{
    njumps = n;
    jump_put = jump_at + n;
    jump_cc_put = jump_cc + n;
}

int jump_op(int op)
{
    int hole;

    if (in_function) {
        if (njumps == jumps_cap) {
            jumps_cap = jumps_cap ? jumps_cap * 2 : 256;
            jump_at = realloc(jump_at, (size_t) jumps_cap * sizeof *jump_at);
            jump_cc = realloc(jump_cc, (size_t) jumps_cap);
            if (!jump_at || !jump_cc)
                acc_error("out of memory for the jumps");
            jumps_rewind(njumps);       /* both of them may have moved */
        }
        *jump_put++ = out_here();
        *jump_cc_put++ = (unsigned char) op;
        njumps++;
    }

    out_opcode24(op, 0);
    hole = out_here() - ACC_INT_SIZE;

    /* Not recorded yet, when this is a function's jump: relax_function says
     * where it ended up, and says it for the ones that are still four bytes
     * only. Almost every relocation a function makes is a jump's operand --
     * 1017 of 1073 in test/bench/compare.c -- so leaving them out until the
     * function is done is what makes shortening them affordable: the pass
     * walked each jump twice over, once as a jump and once as a relocation,
     * and now walks it once. */
    if (!in_function)
        out_reloc(hole);

    return hole;
}

/* The jumps in bytes that are being taken back. A comparison whose answer is
 * only wanted for the branch rewinds the two or three jumps that were making
 * a one and a zero of it, and a sizeof rewinds whatever it compiled to find
 * out how wide something is; either way what was written is not there any
 * more, and neither is what it was. */
void jumps_forget(int here)
{
    while (njumps && jump_put[-1] >= here) {
        jump_put--;
        jump_cc_put--;
        njumps--;
    }
}

/* A jump's hole filled in with where it goes.
 *
 * More than one jump may be waiting on the same place -- a signed comparison
 * jumped on directly needs two, since this chip answers it in two pieces --
 * and they are chained through the holes themselves: each one holds where
 * the one before it is, and the last holds the zero jump_op left. So a
 * caller still has one thing to remember and one thing to patch. */
static void patch_to(int hole, int target)
{
    if (hole && target == out_here())
        join_at = target;               /* see ld_rr_ix */
    while (hole) {
        unsigned char *at = out_img + (hole - out_base);
        int next = get24(at);

        put24(at, target);
        hole = next;
    }
}

void patch_to_here(int hole)
{
    patch_to(hole, out_here());
}

int gen_jump(void)
{
    return jump_op(JP_ANY);
}

/* A jump to somewhere already written: the top of a loop, a label a `goto`
 * has passed, the default of a switch.
 *
 * Written as a hole and filled in at once rather than emitted with the
 * address in it, so that it is recorded with the rest of the jumps and can
 * be shortened with them. These are the ones most likely to be near: the
 * back edge of a loop is as far as the loop is long. */
void gen_jump_to(int target)
{
    patch_to(jump_op(JP_ANY), target);
}

/* Pop the top and jump when it is true (`when_true`) or when it is false.
 *
 * Nothing below the top may be in a register: the code between this jump and
 * wherever it lands runs on one path and not the other, so a value that one
 * path moves and the other does not would be in two places at the join. The
 * callers make sure of it, and it is what lets this use BC freely.
 *
 * A long or a float is first brought down to the 0 or 1 of comparing it with
 * zero at its own type. Testing it as though it were an int reads three of
 * its four bytes, so a long of 0x1000000 was false; and a float is not an
 * integer at all -- -0.0 is false and 0.5 is true, neither of which its bytes
 * say. */
/* The comparison that is true exactly when this one is not. */
static int cmp_opposite(int op)
{
    return op == TK_EQ ? TK_NE
         : op == TK_NE ? TK_EQ
         : op == TK_LT ? TK_GE : TK_LT;
}

/* A jump taken on the flags a comparison's subtract left. `op` is the
 * comparison as the caller wants it: the branch is taken when it holds.
 *
 * All but the signed case are one conditional jump. The signed case is the
 * difficulty cmp_signed has -- the sign is the answer when the subtract did
 * not overflow, and the other way round when it did -- so it is two jumps to
 * the same place, chained through the first's hole. */
static int jump_on_flags(int op, int is_unsigned)
{
    int to_over, first, to_done, second;

    if (op == TK_EQ)
        return jump_op(JP_Z);
    if (op == TK_NE)
        return jump_op(JP_NZ);
    if (is_unsigned)
        return jump_op(op == TK_LT ? JP_C : JP_NC);

    to_over = jump_op(JP_PE);                           /* it overflowed */
    first = jump_op(op == TK_LT ? JP_M : JP_P);
    to_done = jump_op(JP_ANY);
    patch_to_here(to_over);
    second = jump_op(op == TK_LT ? JP_P : JP_M);
    patch_to_here(to_done);
    out_patch24(second, first);         /* the two, as one thing to patch */

    return second;
}

/* The narrow value vderef just read, widened to an int in HL: the high
 * byte is in A, and the widening is what `to` says. For one byte, that is
 * all of it; for two, the low byte is still at (iy+0).
 *
 * Where the widening begins is kept, as vcmp keeps its mark: a conversion
 * straight after to a type as wide -- `(unsigned char) *p` of a char, or a
 * char argument to a char parameter -- needs only the widening that type
 * wants, and the one here is rewound and written again, or kept if it is
 * already the one. */
int  widen_from = -1;    /* where the widening began */
static int  widen_to;           /* and where the value was done */
static Type widen_type;         /* the narrow type it was widened as */
static unsigned widen_epoch;

static void widen_as(Type to)
{
    if (type_unsigned(to))
        fill_hl_with_zero();
    else
        fill_hl_with_sign_of_a();
    if (type_size(to) == 1) {
        ld_l_a();
    } else {
        ld_h_a();
        out_byte3(0xfd, 0x6e, 0x00);    /* ld l, (iy+0) */
    }
}

void widen_loaded(Type to)
{
    widen_from = out_here();
    widen_as(to);
    widen_to = out_here();
    widen_epoch = out_rewinds;
    widen_type = to;
}

/* If `v` is a byte just read and widened into HL, and nothing since: the
 * widening taken back, which leaves the byte in A and HL as it was before
 * -- for a caller about to make all of HL itself from A. */
int widen_undo(const Value *v)
{
    Type type;

    if (!widen_holds(v, &type))
        return 0;
    out_rewind(widen_from);
    widen_from = -1;

    return 1;
}

/* Whether widen_undo would: and if so, the byte's type in *type. */
int widen_holds(const Value *v, Type *type)
{
    if (widen_from < 0 || out_here() != widen_to || widen_epoch != out_rewinds
        || v->kind != VAL_REG || v->val != R_HL || type_size(widen_type) != 1)
        return 0;
    *type = widen_type;

    return 1;
}

/* The same for a binary operator's left side, from before the value stack
 * is in scope. */
int widen_undo_left(void)
{
    return widen_undo(vsp - 2);
}

/* The value on top, if it is a byte just read and widened, stored to the
 * byte at `offset` straight from A, where it still is: the widening taken
 * back, and made again after the store as the value the assignment has --
 * `type`'s -- with the mark gen_discard takes it back by. `*p` into a char
 * was ld a, (hl), the sign fill, ld a, l and the store. Returns 0 if the
 * top is not such a byte. */
int store_byte_widened(int offset, Type type)
{
    Value *top = vsp - 1;

    if (widen_from < 0 || out_here() != widen_to || widen_epoch != out_rewinds
        || top->kind != VAL_REG || top->val != R_HL
        || type_size(widen_type) != 1)
        return 0;
    out_rewind(widen_from);
    widen_from = -1;
    ld_ix_a(offset);
    conversion_from = out_here();
    widen_as(type);
    conversion_to = out_here();
    conversion_epoch = out_rewinds;
    top->type = type_promote(type);
    vset_width(top, type_unsigned(type) ? 1 : 3);

    return 1;
}

/* Whether the value on top is that one, and `to` as wide: if so, it is
 * converted to `to` here. */
int widen_again(Type to)
{
    Value *top = vsp - 1;

    if (widen_from < 0 || out_here() != widen_to || widen_epoch != out_rewinds
        || top->kind != VAL_REG
        || top->val != R_HL || type_size(widen_type) != type_size(to))
        return 0;
    if (type_unsigned(widen_type) != type_unsigned(to)) {
        out_rewind(widen_from);
        widen_as(to);
        widen_to = out_here();
        widen_epoch = out_rewinds;
    }
    widen_type = to;
    top->type = type_promote(to);
    vset_width(top, type_unsigned(to) ? type_size(to) : 3);

    return 1;
}

/* The mark `a && b` and `a || b` leave, as vcmp does: the bytes that make
 * a one or a zero of where the jumps went, which a branch straight after
 * them undoes, to jump on where they went instead. `while (p < e && ok(*p))`
 * then jumps out from each side, rather than making a one or a zero and
 * testing it against zero.
 *
 * The jumps that settled it are taken back into a chain -- patching them
 * wrote over the links that made them one -- so the holes are kept here as
 * they were. A condition with more of them than there is room for is left
 * as it is. */
#define LOGIC_HOLES_MAX 32

int logic_from = -1;     /* where the one or zero began */
static int logic_to;            /* and where it ended */
static int logic_settles;       /* the answer the jumps mean */
static int logic_holes[LOGIC_HOLES_MAX];
static int nlogic_holes;
static unsigned logic_epoch;

/* The holes in a chain, onto the end of the ones kept; 0 when there is no
 * room for them all. */
static int logic_keep(int hole)
{
    while (hole) {
        if (nlogic_holes == LOGIC_HOLES_MAX)
            return 0;
        logic_holes[nlogic_holes++] = hole;
        hole = get24(out_img + (hole - out_base));
    }

    return 1;
}

/* The kept holes as one chain again, and its first. */
static int logic_chain(void)
{
    int i;

    for (i = 0; i != nlogic_holes; i++)
        put24(out_img + (logic_holes[i] - out_base),
              i + 1 < nlogic_holes ? logic_holes[i + 1] : 0);

    return nlogic_holes ? logic_holes[0] : 0;
}

static int jump_on_truth(int when_true)
{
    int reg;

    /* A wide value tested is compared with zero first, and that leaves the
     * flags the mark below reads: a long tested for zero ORs its bytes, and
     * the branch jumps on Z. Asked after the marks, it made its 0 or 1 and
     * tested that again. */
    if (type_wide(vtype()))
        vtruth(TK_NE);

    if (logic_from >= 0 && out_here() == logic_to && logic_epoch == out_rewinds
        && vtop == 1
        && (vsp - 1)->kind == VAL_REG && (vsp - 1)->val == R_HL) {
        int chain = logic_chain(), over;

        out_rewind(logic_from);
        jumps_forget(logic_from);
        logic_from = -1;
        vdrop();

        /* The chain is taken exactly when the answer is `settles`, and
         * falling through is the other answer. */
        if (when_true == logic_settles)
            return chain;
        over = jump_op(JP_ANY);
        patch_to_here(chain);

        return over;
    }

    /* A byte just read or returned, and widened: the whole of it is in A,
     * so the widening goes and A is tested. `while (*p)` and a branch on a
     * function returning bool are these. */
    if (widen_from >= 0 && out_here() == widen_to && widen_epoch == out_rewinds
        && vtop == 1
        && type_size(widen_type) == 1
        && (vsp - 1)->kind == VAL_REG && (vsp - 1)->val == R_HL) {
        out_rewind(widen_from);
        widen_from = -1;
        vdrop();
        or_a_a();

        return jump_op(when_true ? JP_NZ : JP_Z);
    }

    if (cmp_from >= 0 && out_here() == cmp_to && cmp_epoch == out_rewinds && vtop == 1
        && (vsp - 1)->kind == VAL_REG && (vsp - 1)->val == R_HL) {
        int op = when_true ? cmp_op : cmp_opposite(cmp_op);

        out_rewind(cmp_from);
        jumps_forget(cmp_from);
        cmp_from = -1;
        vdrop();

        return jump_on_flags(op, cmp_was_unsigned);
    }

    /* One whose top byte, or top two, are known to be zero: those that are
     * left are tested in A. */
    if (vwidth(vsp - 1) < 3 && (vsp - 1)->kind == VAL_REG) {
        int width = vwidth(vsp - 1);

        reg = vpop_reg();
        if (reg != R_HL)
            mov_rr(R_HL, reg);
        ld_a_l();
        if (width == 2)
            out_byte(0xb4);                     /* or h */
        else
            or_a_a();

        return jump_op(when_true ? JP_NZ : JP_Z);
    }

    reg = vpop_reg();
    if (reg != R_HL)
        mov_rr(R_HL, reg);

    hl_zero_test();

    return jump_op(when_true ? JP_NZ : JP_Z);
}

int gen_jump_if_false(void)
{
    if (vtop != 1)
        acc_error("internal: %d values live at a branch", vtop);

    return jump_on_truth(0);
}

/* The bottom of a do-while: back to the top while the condition holds. */
void gen_jump_if_true_to(int target)
{
    if (vtop != 1)
        acc_error("internal: %d values live at a branch", vtop);

    patch_to(jump_on_truth(1), target);
}

/* The value a switch compares its cases with, into HL -- and for a long its
 * top byte into A -- once, ahead of all the tests. */
void gen_switch_load(int slot, Type type)
{
    ld_rr_ix(R_HL, slot);
    if (type_wide(type)) {
        ld_a_ix(slot + ACC_INT_SIZE);
    }
}

/* To `target` if the loaded value is `value`.
 *
 *   ld de, value / or a / sbc hl, de / add hl, de / jp z, target
 *
 * The add puts HL back for the next case and leaves the zero flag as the
 * subtraction set it, so the value is loaded once however many cases there
 * are. A long compares its top byte in A first and skips the rest when that
 * differs. */
void gen_switch_case(long value, uint32_t high, Type type, int target,
                     int slot)
{
    /* A long long compares its top five bytes from the frame, one at a time,
     * and the low three in HL as anything else does. Each miss jumps past
     * the rest of the case: the groups after it, seven bytes each, and the
     * twelve of the tail. */
    if (type_eight(type)) {
        int k;

        for (k = 7; k >= ACC_INT_SIZE; k--) {
            uint32_t half = k >= 4 ? high : (uint32_t) value;

            ld_a_ix(slot + k);
            out_byte2(0xfe, (int) (half >> (k % 4 * 8)) & 0xff);   /* cp n */
            out_byte2(0x20, 12 + (k - ACC_INT_SIZE) * 7);   /* jr nz */
        }
    } else if (type_wide(type)) {
        out_byte2(0xfe, (int) ((unsigned long) value >> 24) & 0xff);  /* cp n */
        out_byte2(0x20, 12);                    /* jr nz, past the rest */
    }
    out_byte(0x11);                             /* ld de, value */
    out_word24((int) (value & 0xffffff));
    or_a_a();
    sbc_hl_rr(R_DE);
    add_hl_rr(R_DE);
    out_reloc(out_here() + 1);
    out_opcode24(JP_Z, target);
}

/* The top becomes 0 or 1 according to how it compares with zero -- TK_NE for
 * its truth, TK_EQ for `!`. The zero is of the value's own kind, so the
 * comparison is a float one for a float and a four-byte one for a long, and a
 * pointer compares with the null pointer as the unsigned int it is. */
/* The top as a _Bool: 0 when it is zero, 1 when it is not -- a float at
 * either zero, a long in all four bytes, a pointer that is null. */
__attribute__((noinline))
void bool_from(void)
{
    vtruth(TK_NE);
}

void vtruth(int op)
{
    /* Straight after a comparison, or an AND that left its mark, the flags
     * are still there to be read: `!(a < b)` is a >= b, and `!(c & 0x80)`
     * is the AND's zero flag, with no compare against zero in between. */
    if (cmp_from >= 0 && out_here() == cmp_to && cmp_epoch == out_rewinds
        && (vsp - 1)->kind == VAL_REG && (vsp - 1)->val == R_HL) {
        int now = op == TK_EQ ? cmp_opposite(cmp_op) : cmp_op;

        out_rewind(cmp_from);
        jumps_forget(cmp_from);
        vdrop();
        cmp_value(now, cmp_was_unsigned);

        return;
    }

    if (type_float(vtype()))
        vpush_const_float(0.0f);
    else
        vpush_const(0, TY_INT);
    vapply(op, 0);
}

/* `a && b` and `a || b`, in two halves around the right operand.
 *
 * Each stops as soon as its answer is known: a false left side settles `&&`
 * and a true one settles `||`, and the right side is then never evaluated --
 * which C guarantees, and which a program relies on whenever the right side
 * would be an error if the left had gone the other way. `settles` is the truth
 * value that decides the whole thing early: 0 for `&&`, 1 for `||`.
 *
 * Everything below the left operand goes to the frame first, so that no
 * register holds a value that only one path knows about. */
int gen_logic_left(int settles)
{
    save_regs_below(1);

    return jump_on_truth(settles);
}

void gen_logic_right(int settles, int early)
{
    int late, done, from, kept;

    save_regs_below(1);
    late = jump_on_truth(settles);

    nlogic_holes = 0;
    kept = logic_keep(early) && logic_keep(late);
    from = out_here();

    ld_rr_imm(R_HL, !settles);          /* neither side settled it */
    done = jump_op(JP_ANY);

    patch_to_here(early);
    patch_to_here(late);
    ld_rr_imm(R_HL, settles);           /* one of them did */

    patch_to_here(done);
    vpush_reg(R_HL);

    logic_from = kept ? from : -1;
    logic_to = out_here();
    logic_epoch = out_rewinds;
    logic_settles = settles;
}

void gen_label(int hole)
{
    patch_to_here(hole);
}

/* ------------------------------------------------------------------ */
/* ?:                                                                  */

/* The type the two sides of `?:` meet at. Arithmetic types by the usual
 * conversions, promoted to at least int; two pointers only if they agree about
 * what they point at, and a pointer with a literal 0, which is the null
 * pointer spelled the way C spells it. */
/* In the middle's extension word, beside its extension and qualifiers:
 * that it was a struct. */
#define COND_STRUCT 0x10000

static Type cond_type(Type a, int a_null, Type b, int b_null)
{
    if (type_pointer(a) || type_pointer(b)) {
        if (a == b)
            return a;
        if (type_pointer(a) && b_null)
            return a;
        if (type_pointer(b) && a_null)
            return b;

        /* A pointer to an object and a `void *` meet at `void *` (C99
         * 6.5.15p6): c99-const-expr-4's `j ? p : n`, where n is a const
         * `void *` that is 0 but not a null pointer constant. */
        if (type_pointer(a) && type_pointer(b)
            && (type_deref(a) == TY_VOID || type_deref(b) == TY_VOID))
            return type_ptr_to(TY_VOID);
        acc_error_at(tok_line, "the two sides of ?: are %s",
                     type_pointer(a) && type_pointer(b)
                         ? "pointers to different types"
                         : "a pointer and a number that is not 0");
    }

    if (type_wide(a) || type_wide(b))
        return common_wide(a, b);

    a = type_promote(a);
    b = type_promote(b);

    return (type_unsigned(a) || type_unsigned(b)) ? TY_UINT : TY_INT;
}

/* Where an answer waits for the join: in HL if it fits in a register, in the
 * slot reserved for it if it does not. Both paths put theirs in the same
 * place, which is what lets one descriptor stand for it afterwards. */
static void cond_park(int slot)
{
    Value *top = vsp - 1;

    if (type_wide(top->type))
        materialise_long(slot, top->type);
    else
        force_into(top, R_HL);
    vdrop();
}

/* After the condition: a home for the answer, and a jump to the third
 * operand when the condition is false. Everything below the condition goes
 * to the frame first, because from here the two paths diverge. */
int gen_cond_begin(int *slot, int *lock)
{
    *lock = spill_locked;
    *slot = long_scratch(TY_LONG);

    /* Nothing else may build in the scratch below where the middle operand
     * is about to be parked, because the third operand is compiled over the
     * top of it and the stack will not be saying that the slot is taken. */
    spill_locked = spill_used;
    save_regs_below(1);

    return jump_on_truth(0);
}

/* After the middle operand: park it as the type it is, and jump forward to a
 * stub that does not exist yet. What the answer's type is depends on the
 * third operand, which has not been parsed, so converting now would be
 * guessing. */
/* That the two sides of a ?: agree about being a struct, which C asks for
 * however the condition turned out. The folded path keeps one side and
 * throws the other away, so without this `1 ? s : t` on two different
 * structs was accepted for the one reason that nothing looked at t. */
void gen_cond_same(Type middle, int middle_ext)
{
    const Value *top = vsp - 1;
    int both = type_is_struct(middle);

    if (type_is_struct(top->type) != both
        || (both && top->ext != (middle_ext & 0xff)))
        acc_error_at(tok_line, "the two sides of ?: have to be the same struct "
                               "or union, or neither be one");
}

/* The same two, where both sides are void: `c ? f() : g()`, which C says is
 * itself void. There is nothing to carry across the join, so there is no
 * slot to park it in and no type to reconcile -- only the two paths, one of
 * which runs. acc's own `expect` is written that way, and parking a value
 * that does not exist is what stopped acc compiling its own parser. */
int gen_cond_middle_void(void)
{
    vdrop();                            /* what the middle did, not a value */

    return jump_op(JP_ANY);
}

void gen_cond_end_void(int to_stub, int lock)
{
    spill_locked = lock;
    if (vtype() != TY_VOID)
        acc_error_at(tok_line, "one side of ?: gives a value and the other "
                               "is void, so there is no answer to give");
    vdrop();
    gen_label(to_stub);
    vpush(VAL_VOID, TY_VOID, 0);
}

int gen_cond_middle(int *slot, Type *middle, int *middle_ext, int *middle_null)
{
    Value *top = vsp - 1;

    /* A struct goes through the join as its address, which is its value:
     * relabelled as a pointer so that it can be parked in HL, and marked
     * so that the end can relabel it back. */
    int was_struct = type_is_struct(top->type) ? COND_STRUCT : 0;

    if (was_struct)
        top->type = type_ptr_to(TY_STRUCT);
    *middle = top->type;
    *middle_ext = top->ext | top->quals << 8 | was_struct;  /* all, in one */
    *middle_null = (val_const(top->kind) && top->val == 0);

    /* The slot begin made is a long's; a long long needs one of its own. */
    if (type_eight(top->type))
        *slot = long_scratch(top->type);
    cond_park(*slot);

    return jump_op(JP_ANY);
}

/* After the third operand, when both types are known. The third converts
 * and parks where it is; then the stub the middle jumped to is written, and
 * it finds the middle's value exactly where it was left -- nothing between
 * the jump and here ran on that path -- converts it the same way, and parks
 * it in the same place. */
void gen_cond_end(int to_stub, int slot, int lock, Type middle,
                  int middle_ext,
                  int middle_null)
{
    spill_locked = lock;
    Value *top = vsp - 1;
    int was_struct = middle_ext & COND_STRUCT;
    int third_null;

    if (type_is_struct(top->type) != (was_struct != 0)
        || (was_struct && top->ext != (middle_ext & 0xff)))
        acc_error_at(tok_line, "the two sides of ?: have to be the same struct "
                               "or union, or neither be one");
    if (was_struct)
        top->type = type_ptr_to(TY_STRUCT);
    third_null = (val_const(top->kind) && top->val == 0);
    Type result = cond_type(middle, middle_null, top->type, third_null);
    int ext = third_null ? middle_ext & 0xff : top->ext; /* the side not 0 */
    /* Const if either side is; as narrow as both sides are, and no more. */
    int quals = ((middle_ext >> 8 | top->quals) & 0xff & ~(VQ_BYTE | VQ_WORD))
                | (middle_ext >> 8 & top->quals & (VQ_BYTE | VQ_WORD));
    int done, park = slot;

    /* A long long answer from a middle that was not one: where the middle
     * was parked is too small, so the answer goes somewhere new. */
    if (type_eight(result) && !type_eight(middle))
        park = long_scratch(result);

    vconvert(result);
    cond_park(park);
    done = jump_op(JP_ANY);

    patch_to_here(to_stub);
    if (type_wide(middle))
        vpush_scratch(middle, slot);
    else
        vpush(VAL_REG, type_promote(middle), R_HL);
    vconvert(result);
    cond_park(park);

    patch_to_here(done);
    if (type_wide(result))
        vpush_scratch(result, park);
    else
        vpush(VAL_REG, result, R_HL);
    (vsp - 1)->ext = (unsigned char) ext;
    (vsp - 1)->quals = (unsigned char) quals;
    if (was_struct)
        (vsp - 1)->type = TY_STRUCT;
}
