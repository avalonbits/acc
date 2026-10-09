/*
 * Arithmetic on ints and the types narrower: folding constants, the binary
 * and unary operators, the byte-wide forms, the comparisons, and applying
 * an operator the parser has read.
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

static int  is_comparison(int op);
static void flags_say_nonzero(int from);

static int bitwise_const(int op, int value)
{
    int c0 = value & 0xff, c1 = (value >> 8) & 0xff, c2 = (value >> 16) & 0xff;
    int identity = op == TK_AMP ? 0xff : 0x00;
    int low, high, at;

    /* A mask that keeps nothing above the low byte, which is what nearly
     * every AND in a program is: whatever the two bytes above held, the
     * answer there is zero, and sbc hl, hl says so in two bytes because AND
     * has just cleared the carry. */
    if (op == TK_AMP && c1 == 0x00 && c2 == 0x00) {
        if (c0 == 0x00) {
            fill_hl_with_zero();

            return 1;
        }
        if (!widen_undo_left())
            ld_a_l();           /* unless the byte is in A already */
        and_a_imm(c0);
        at = out_here();
        sbc_hl_hl();            /* and cleared the carry, so this is 0 */
        ld_l_a();
        flags_say_nonzero(at);
        widen_made(at, TY_UCHAR);       /* a byte, in A still */

        return 1;
    }

    /* A mask that keeps something of the middle byte and nothing above it:
     * `& 0x8000`, `& 0xffff`. The third byte is cleared the same way, with
     * sbc hl, hl after the AND that leaves the carry clear -- which would
     * clear the low byte too, so a low byte that is kept waits in IY, the
     * backend's own scratch, as the narrowing to a short keeps its low byte.
     * A mask with nothing in the low byte needs no waiting, and is six
     * bytes against the eight of loading BC and calling. crc16 in
     * test/perf's crc.c tests `c & 0x8000` eight times a byte. */
    if (op == TK_AMP && c2 == 0x00) {
        if (c0 != 0x00) {
            ld_a_l();
            if (c0 != 0xff)
                and_a_imm(c0);
            iy_save();
            out_byte2(0xfd, 0x6f);      /* ld iyl, a */
        }
        ld_a_h();
        if (c1 == 0xff)
            or_a_a();                   /* the carry, cleared in a byte */
        else
            and_a_imm(c1);              /* which clears it too */
        at = out_here();
        sbc_hl_hl();
        ld_h_a();
        if (c0 != 0x00) {
            out_byte2(0xfd, 0x7d);      /* ld a, iyl */
            iy_restore();
            ld_l_a();
        } else {
            flags_say_nonzero(at);
        }

        return 1;
    }

    /* Otherwise the third byte has to be left alone, because there is no way
     * to name it: what the operator would do to it must be nothing. */
    if (c2 != identity)
        return 0;

    low = c0 != identity;
    high = c1 != identity;
    if (!low && !high)
        return 1;               /* the operator would change nothing */
    if (!high && op != TK_AMP
        && widen_byte_op(vsp - 2, op == TK_PIPE ? 0xf6 : 0xee, c0))
        return 1;               /* on the byte, the widening after */

    if (low) {
        ld_a_l();
        if (op == TK_AMP)
            and_a_imm(c0);
        else if (op == TK_PIPE)
            or_a_imm(c0);
        else
            xor_a_imm(c0);
        ld_l_a();
    }
    if (high) {
        ld_a_h();
        if (op == TK_AMP)
            and_a_imm(c1);
        else if (op == TK_PIPE)
            or_a_imm(c1);
        else
            xor_a_imm(c1);
        ld_h_a();
    }

    return 1;
}

static int mul_const(int value)
{
    const unsigned *p, *q;
    unsigned rest;
    int top, pc = 1, steps;

    if (value <= 0 || value > 0xffff)
        return 0;               /* zero and one are folded before this */

    /* The bits are found by walking down the powers of two and taking
     * each that still fits, rather than as `value & (1 << bit)`: on this
     * target a shift and an AND of an int are each a call into the
     * runtime, and finding the top bit that way, down from 23, cost two
     * dozen of each for every constant multiply -- which every subscript
     * of an array of anything wider than a char is. A compare and a
     * subtract are instructions. */
    for (p = powers_of_two + 15, top = 15; (unsigned) value < *p; p--)
        top--;
    rest = (unsigned) value - *p;
    for (q = p; q > powers_of_two && rest; ) {
        q--;
        if (rest >= *q) {
            rest -= *q;
            pc++;
        }
    }

    steps = top + (pc - 1);
    if (steps > MUL_MAX_STEPS)
        return 0;

    if (pc == 1) {              /* a power of two: doublings and nothing else */
        while (top--)
            add_hl_hl();

        return 1;
    }

    push_rr(R_DE);
    push_rr(R_HL);
    pop_rr(R_DE);               /* de = x, whatever de held is under it */
    rest = (unsigned) value - *p;
    for (q = p; q > powers_of_two; ) {
        q--;
        add_hl_hl();
        if (rest >= *q) {
            rest -= *q;
            add_hl_rr(R_DE);
        }
    }
    pop_rr(R_DE);

    return 1;
}

/* ------------------------------------------------------------------ */
/* operators                                                           */

/* A value as the target would hold it.
 *
 * acc folds constants in the host's int, which is 24 bits when it runs on the
 * Agon and 32 when it is cross-compiling. Addition and subtraction do not care
 * -- two's complement gives the same bits at any width, and the emitter writes
 * three bytes either way -- but division, remainder, a signed shift and every
 * comparison do. Narrowing here, where the value is produced, means the stack
 * always holds what the machine would have, and those operations get the right
 * answer when they arrive rather than a bug to find later. */
int trunc_int(int value)
{
    value &= 0xffffff;
    if (value & 0x800000)
        value -= 0x1000000;

    return value;
}

/* An int's bits as the unsigned value they are: the stack holds every int
 * sign-extended, which is the signed reading, and 0xffffffU is -1 there. */
#define as_unsigned(v)  ((unsigned long) (v) & 0xffffffUL)

static int const_fold(int op, int left, int right, int *out, int is_unsigned)
{
    /* Where signed and unsigned disagree -- the ordering, a right shift, a
     * division -- an unsigned operand is read as the unsigned value its bits
     * are. Folded on the sign-extended value, 0x800000U / 2U was -0x400000,
     * and -1U >> 9 was -1: the arithmetic shift a signed value gets. */
    if (is_unsigned) {
        unsigned long l = as_unsigned(left), r = as_unsigned(right);

        switch (op) {
        case TK_LT:    *out = l <  r; return 1;
        case TK_GT:    *out = l >  r; return 1;
        case TK_LE:    *out = l <= r; return 1;
        case TK_GE:    *out = l >= r; return 1;
        case TK_SHR:
            if (right < 0 || right >= ACC_INT_SIZE * 8)
                return 0;
            *out = trunc_int((int) (l >> right));
            return 1;
        case TK_SLASH:
            if (r == 0)
                return 0;
            *out = trunc_int((int) (l / r));
            return 1;
        case TK_PERCENT:
            if (r == 0)
                return 0;
            *out = trunc_int((int) (l % r));
            return 1;
        }
    }

    switch (op) {
    case TK_PLUS:  *out = trunc_int(left + right); return 1;
    case TK_MINUS: *out = trunc_int(left - right); return 1;
    case TK_LT:    *out = left <  right; return 1;
    case TK_GT:    *out = left >  right; return 1;
    case TK_LE:    *out = left <= right; return 1;
    case TK_GE:    *out = left >= right; return 1;
    case TK_EQ:    *out = left == right; return 1;
    case TK_NE:    *out = left != right; return 1;
    case TK_AMP:   *out = left & right; return 1;
    case TK_PIPE:  *out = left | right; return 1;
    case TK_CARET: *out = left ^ right; return 1;

    /* Shifts fold only where C defines them. A negative or over-wide count is
     * undefined, and folding it would bake this host's answer into a program
     * that has to run on the Agon. */
    case TK_SHL:
        if (right < 0 || right >= ACC_INT_SIZE * 8)
            return 0;
        /* On the bits: shifting a negative value left is undefined in the C
         * that compiles acc, whatever it would come to on the Agon. */
        *out = trunc_int((int) ((unsigned) left << right));
        return 1;

    case TK_SHR:
        if (right < 0 || right >= ACC_INT_SIZE * 8)
            return 0;
        *out = left >> right;
        return 1;

    case TK_STAR:  *out = trunc_int(left * right); return 1;

    /* Division by zero is undefined, and folding it would make the compiler
     * trap on a program that might never reach the expression. */
    case TK_SLASH:
        if (right == 0)
            return 0;
        *out = trunc_int(left / right);
        return 1;

    case TK_PERCENT:
        if (right == 0)
            return 0;
        *out = trunc_int(left % right);
        return 1;
    }

    return 0;
}

/* C's usual arithmetic conversions, as far as this compiler's types go: both
 * sides are already int-wide by the time they are in registers, and if either
 * is unsigned the result and any comparison are unsigned too. */
static int either_unsigned(const Value *lhs, const Value *rhs)
{
    return type_unsigned(lhs->type) || type_unsigned(rhs->type);
}

/* Whether a value is known not to be negative: an unsigned type narrower
 * than int, which promotes to a non-negative int, or a constant >= 0. */
static int never_negative(const Value *v)
{
    return (type_unsigned(v->type) && type_size(v->type) < ACC_INT_SIZE)
           || (val_number(v->kind) && v->val >= 0);
}

/* Whether a comparison is unsigned. The two sides may not have been
 * loaded yet, so their types are not yet promoted: an unsigned char
 * promotes to int, and `u < -1` is false for every u. But two sides that
 * are neither of them negative order the same either way, and the
 * unsigned comparison is the shorter. */
static int cmp_is_unsigned(const Value *lhs, const Value *rhs)
{
    return type_unsigned(type_promote(lhs->type))
           || type_unsigned(type_promote(rhs->type))
           || (never_negative(lhs) && never_negative(rhs));
}

/* Whether an operator's operands are read as unsigned: either of them for
 * arithmetic, and only the left for a shift, whose count's type C99 6.5.7
 * says nothing about the result's. */
static int fold_unsigned(int op, const Value *lhs, const Value *rhs)
{
    if (tok_pair(op, TK_SHL))
        return type_unsigned(type_promote(lhs->type)) != 0;

    return either_unsigned(lhs, rhs);
}

static void no_addr_arithmetic(void)
{
    acc_error_at(tok_line, "only adding a number to an address, or taking "
                           "one from it, gives something that still moves "
                           "with the program, and this does not");
}

/* Whether the answer is still an address, given an operator and two
 * constants -- and a refusal when it is neither an address nor a number that
 * means the same wherever the program is put.
 *
 * A tagged constant is the base the image is loaded at plus so much. Adding a
 * number to it, or taking one from it, changes the "so much" and leaves an
 * address, which a relocation moves. One address taken from another is the
 * distance between them, and that is the same at every base, so it is an
 * ordinary number.
 *
 * Nothing else has an answer that moving the program could put right: `&a *
 * 2` would move twice as far as the image does, `&a & 255` not at all, and
 * `&a + &b` twice. Each of those is a number that silently depends on where
 * the program was loaded, and the one thing a compiler that is about to gain
 * a linker must not do is write one of them down. At run time they all
 * still work, because then the address in the image is a whole one, which
 * is how one in a function is done: -1 says it cannot be folded, and in a
 * function the operation is then emitted like any other. In a global's
 * initial value, which has to be bytes now, it is refused. */
static int fold_addr(int op, const Value *lhs, const Value *rhs)
{
    int left = lhs->kind, right = rhs->kind;

    if (!val_pending(left) && !val_pending(right))
        return VAL_CONST;

    /* A number added to one of them, or taken from one, moves it along and
     * leaves it the kind it was. */
    if (op == TK_PLUS && !val_pending(right))
        return left;
    if (op == TK_PLUS && !val_pending(left))
        return right;
    if (op == TK_MINUS && val_pending(left) && !val_pending(right))
        return left;

    /* And one taken from another of the same kind is the distance between
     * them, which is the same wherever the two of them end up. Two of
     * different kinds are not: one is in the image and one is past its end,
     * and how far apart they are is not known until the image is finished. */
    if (op == TK_MINUS && left == right)
        return VAL_CONST;

    return -1;
}

/* Whether both sides can be worked out now.
 *
 * A pending address carries where it will be and not what it is worth, so
 * it can be folded only against something that moves with it: a plain
 * number added to it or taken from it, or another address of the same kind,
 * whose distance from it is settled however the two of them are placed.
 *
 * Against a number in any other way it cannot. The first object in the bss
 * is at offset zero, and folding `first == 0` on the offsets decided it was
 * the null pointer.
 *
 * Only comparisons ask. The arithmetic ones fold through fold_addr, which
 * knows which kinds an address survives and says so about the rest -- and
 * saying so is a better message than anything reached by declining to fold
 * and letting whatever wanted a constant complain instead. */
static int foldable(int op, const Value *lhs, const Value *rhs)
{
    int pending = val_pending(lhs->kind) + val_pending(rhs->kind);

    if (!pending)
        return 1;
    if (op == TK_PLUS || op == TK_MINUS)
        return 1;               /* fold_addr says what kind those leave */

    return pending == 2 && lhs->kind == rhs->kind;
}

/* HL = HL OP BC, a byte or two at a time in A, when the widths say the
 * bytes above those are zero in the answer -- see vwidth. Returns the
 * answer's width, or 0 for one this cannot do, which the helper then does.
 *
 * An AND is as wide as the narrower side, and the bytes of the wider side
 * above that are cleared: with sbc hl, hl after an `and`, which clears the
 * carry, or through IY for the low byte when the middle one is kept, as
 * bitwise_const does. An OR or an XOR is as wide as the wider side, and
 * both have to be narrow, so that the top byte of HL is zero already and
 * stays so. */
static int bitwise_narrow(int op, int lw, int rw)
{
    int alu = op == TK_AMP ? 0xa0 : op == TK_PIPE ? 0xb0 : 0xa8;
    int width = op == TK_AMP ? (lw < rw ? lw : rw) : (lw > rw ? lw : rw);

    if (width == 3)
        return 0;
    if (width == 1) {
        ld_a_l();
        out_byte(alu + 1);                      /* op c */
        if (lw > 1)
            sbc_hl_hl();                        /* an AND: its carry is clear */
        ld_l_a();

        return 1;
    }
    if (lw <= 2) {
        ld_a_l();
        out_byte(alu + 1);                      /* op c */
        ld_l_a();
        ld_a_h();
        out_byte(alu);                          /* op b */
        ld_h_a();

        return 2;
    }
    ld_a_l();                                   /* an AND, of three by two */
    out_byte(alu + 1);                          /* and c */
    iy_save();
    out_byte2(0xfd, 0x6f);                      /* ld iyl, a */
    ld_a_h();
    out_byte(alu);                              /* and b */
    sbc_hl_hl();
    ld_h_a();
    out_byte2(0xfd, 0x7d);                      /* ld a, iyl */
    iy_restore();
    ld_l_a();

    return 2;
}

/* Where the last load of an address the link fills in ended, and into
 * which register: see vpush_global_addr and vbinop. */
int      gaddr_end = -1, gaddr_reg;
unsigned gaddr_epoch;

void vbinop(int op)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;
    int folded, right, lw, rw, narrow = 0;
    Type result, lhs_type;

    if ((unsigned) vtop < 2)
        acc_error("internal: binary operator with nothing to work on");

    /* Both sides known: the answer is known, and nothing is emitted. A
     * shift takes its type from the left operand alone, as the code below
     * does when it is not folded: `-64 >> 3U` is a signed -8. */
    if (val_const(lhs->kind) && val_const(rhs->kind)
        && const_fold(op, lhs->val, rhs->val, &folded,
                      fold_unsigned(op, lhs, rhs))) {
        Type folded_type = fold_unsigned(op, lhs, rhs) ? TY_UINT : TY_INT;
        /* The kinds are in hand from the test above, and neither is waiting
         * on anything in almost every fold a program does, so the question
         * is asked here and the answer worked out elsewhere. */
        int kind = val_pending(lhs->kind) || val_pending(rhs->kind)
                   ? fold_addr(op, lhs, rhs) : VAL_CONST;

        if (kind >= 0) {
            vdrop();
            vdrop();
            vpush_const(folded, folded_type);
            if (kind != VAL_CONST)
                (vsp - 1)->kind = (unsigned char) kind;

            return;
        }
        if (!in_function)
            no_addr_arithmetic();

        /* Loaded, relocated, before anything below takes either side for
         * an immediate: the number in the Value is where the address will
         * be relative to the image, which is not what it is worth. */
        if (val_pending(lhs->kind))
            force_reg(lhs);
        if (val_pending(rhs->kind))
            force_reg(rhs);
    }

    /* A constant on the left of an operator that does not care which side
     * is which goes to the right, where what follows takes it as an
     * immediate -- and the other side, which is often in HL already, stays
     * there. `table[i]` is the table's address plus the index: loaded the
     * other way round, the index was moved out of HL to make room for the
     * address, and moved back to be added to it. */
    if (val_const(lhs->kind) && !val_const(rhs->kind)
        && (op == TK_PLUS || op == TK_STAR || op == TK_AMP
            || op == TK_PIPE || op == TK_CARET)) {
        vswap();
        lhs = vsp - 2;
        rhs = vsp - 1;
    }

    /* Adding or subtracting nothing is nothing. Worth the two lines: it is
     * what makes `p + 0` and the zero cases of generated code free. */
    if (val_number(rhs->kind) && rhs->val == 0
        && (op == TK_PLUS || op == TK_MINUS)) {
        vdrop();

        return;
    }

    /* The local in IY and a constant added to it or taken from it: still IY,
     * the difference kept as the displacement a use reads -- (iy+d) or
     * lea rr, iy+d -- so `p + 1` is no code at all. */
    if ((op == TK_PLUS || op == TK_MINUS) && lhs->kind == VAL_IY
        && val_number(rhs->kind) && !type_float(lhs->type)) {
        int d = lhs->val + (op == TK_PLUS ? rhs->val : -rhs->val);

        if (disp_fits(d)) {
            result = either_unsigned(lhs, rhs) ? TY_UINT : TY_INT;
            vdrop();
            (vsp - 1)->val = d;
            (vsp - 1)->type = result;

            return;
        }
    }

    /* An address the link fills in, just loaded, and a constant added to
     * it or taken from it: the constant goes into the slot, which the link
     * adds the address to -- `g.b` or `arr[3]` of an extern is one ld hl,
     * not an ld hl, an ld de and an add. */
    if ((op == TK_PLUS || op == TK_MINUS) && val_number(rhs->kind)
        && lhs->kind == VAL_REG && lhs->val == gaddr_reg
        && out_here() == gaddr_end && gaddr_epoch == out_rewinds) {
        int at = gaddr_end - ACC_INT_SIZE;
        Type type = lhs->type;

        out_add24(at, op == TK_PLUS ? rhs->val : -rhs->val);
        vdrop();
        (vsp - 1)->type = type;

        return;
    }

    /* Stepping by a little, one instruction at a time. `inc hl` is one byte
     * and one cycle; putting the same amount in BC and adding it is five of
     * each, because the immediate is the full width of a register here. So
     * up to four steps is cheaper both ways, and `p++` and `i++` and every
     * walk along a string are one step.
     *
     * The carry that `add hl, rr` would leave is not left, and nothing wants
     * it: what this produces is a value, and every branch tests the value it
     * is given rather than flags it inherited. */
    if (rhs->kind == VAL_CONST && (op == TK_PLUS || op == TK_MINUS)
        && rhs->val >= -STEP_MAX && rhs->val <= STEP_MAX
        && !type_float(lhs->type)) {
        int n = op == TK_PLUS ? rhs->val : -rhs->val;

        force_into(vsp - 2, R_HL);
        while (n > 0) {
            inc_hl();
            n--;
        }
        while (n < 0) {
            dec_hl();
            n++;
        }
        result = either_unsigned(lhs, rhs) ? TY_UINT : TY_INT;
        vdrop();
        vdrop();
        vpush_reg(R_HL);
        (vsp - 1)->type = result;

        return;
    }

    /* A commutative operator whose right side is in HL already and whose
     * left side is in another register is done the other way round, so
     * that HL stays where it is. `t[c]` with the table's address in DE and
     * c in HL moved c to BC through the stack and swapped DE and HL, to add
     * them: `add hl, de` is the same sum. */
    if ((op == TK_PLUS || op == TK_STAR || op == TK_AMP || op == TK_PIPE
         || op == TK_CARET)
        && rhs->kind == VAL_REG && rhs->val == R_HL && lhs->kind == VAL_REG) {
        vswap();
        lhs = vsp - 2;
        rhs = vsp - 1;
    }

    /* add hl, rr and sbc hl, rr only accumulate into HL, so the left operand
     * goes there and the right one goes anywhere else. Both are done through
     * the allocator rather than by moving registers about by hand: a scratch
     * register chosen without asking whether anything already lives in it is
     * how `f(a,b,c) + f(1,2,3)` lost an argument. */
    force_into(vsp - 2, R_HL);

    /* A right shift, divide or remainder by a constant that shr_const
     * writes out, answering its type, or makes an AND. */
    if (val_number(rhs->kind)
        && (op == TK_SHR || tok_pair(op, TK_SLASH))) {
        int how = shr_const(op, lhs, rhs);

        if (how == 1) {
            op = TK_AMP;
        } else if (how) {
            result = (Type) how;
            goto done;
        }
    }

    /* A left shift by a constant of up to eight is add hl, hl a bit at a
     * time, a byte each: no bigger than loading the count and calling, and
     * without the helper's loop. crc16 in test/perf's crc.c shifts by 1
     * eight times a byte and by 8 once, and those two calls were a third of
     * its inner loop. */
    if (op == TK_SHL && val_number(rhs->kind)
        && rhs->val >= 0 && rhs->val <= 8) {
        int count = rhs->val;

        result = type_unsigned(type_promote(lhs->type)) ? TY_UINT : TY_INT;
        while (count-- > 0)
            add_hl_hl();
        vdrop();
        vdrop();
        vpush_reg(R_HL);
        (vsp - 1)->type = result;

        return;
    }

    /* A bitwise operator with a constant on the right is written out here
     * rather than called for: see bitwise_const. */
    if (val_const(rhs->kind)
        && ((op == TK_AMP || op == TK_PIPE || op == TK_CARET)
            ? bitwise_const(op, rhs->val)
            : op == TK_STAR && mul_const(rhs->val))) {
        result = either_unsigned(lhs, rhs) ? TY_UINT : TY_INT;
        vdrop();
        vdrop();
        vpush_reg(R_HL);
        (vsp - 1)->type = result;

        return;
    }

    if (needs_helper(op)) {
        /* The helpers take their right operand in BC, by the convention
         * agondev uses for the same operations. */
        force_into(vsp - 1, R_BC);
        right = R_BC;
    } else {
        right = force_reg(vsp - 1);
    }


    lhs_type = type_promote(lhs->type);
    result = either_unsigned(lhs, rhs) ? TY_UINT : TY_INT;

    /* A shift's result takes its type from the left operand alone: `1u >> x`
     * is unsigned and `1 >> u` is not, which is C's rule and not the usual
     * arithmetic conversions. */
    if (tok_pair(op, TK_SHL))
        result = type_unsigned(lhs_type) ? TY_UINT : TY_INT;

    switch (op) {
    case TK_PLUS:
        add_hl_rr(right);
        break;

    case TK_MINUS:
        or_a_a();               /* sbc reads the carry, so clear it */
        sbc_hl_rr(right);
        break;

    /* No instruction does any of these on a 24-bit value, so they go to a
     * helper acc emits into the image. The helper takes its right operand in
     * BC, which is where force_into has just put it. */
    case TK_AMP:
    case TK_PIPE:
    case TK_CARET:
        lw = vwidth(vsp - 2);           /* loaded, so each says what it is */
        rw = vwidth(vsp - 1);
        narrow = bitwise_narrow(op, lw, rw);
        if (!narrow)
            rt_call(op == TK_AMP ? RT_AND : op == TK_PIPE ? RT_OR : RT_XOR);
        break;
    case TK_SHL:   rt_call(RT_SHL); break;
    case TK_SHR:
        rt_call(type_unsigned(lhs_type) ? RT_SHRU : RT_SHRS);
        break;

    /* Signed and unsigned multiply agree in two's complement, so there is
     * one routine. */
    case TK_STAR:  rt_call(RT_MUL); break;

    /* Division does care which it is: -7 / 2 is -3 and 16777209 / 2 is
     * 8388604, from the same twenty-four bits. */
    case TK_SLASH:
        rt_call(type_unsigned(result) ? RT_DIVU : RT_DIVS);
        break;

    case TK_PERCENT:
        rt_call(type_unsigned(result) ? RT_REMU : RT_REMS);
        break;

    default:
        acc_error("the operator %s is not implemented yet", tok_spelling(op));
    }

done:
    vdrop();
    vdrop();
    vpush_reg(R_HL);
    (vsp - 1)->type = result;
    if (narrow)
        vset_width(vsp - 1, narrow);
}

/* Assignment in C has a value, so the stored value stays on the stack. The
 * caller drops it when it is a statement and keeps it when it is not, which
 * is what makes `a = b = 0` work without a special case. */
/* How many side effects have been compiled: stores, steps and calls. The
 * parser reads it before and after an expression it is about to throw away,
 * to know whether throwing it away loses anything. */
int gen_effects;

/* The conversion an assignment to a narrow object leaves on its value,
 * written after the store, and where it ended. */
int conversion_from = -1;
int conversion_to;
unsigned conversion_epoch;

/* Drop a value that nothing will read: a statement's, or the left side of a
 * comma. When it is still the one an assignment to a narrow object just
 * converted, the conversion goes too. */
void gen_discard(void)
{
    /* A volatile local read though nothing uses it, as the program says. */
    if (((vsp - 1)->quals & VQ_VOLATILE) && (vsp - 1)->kind == VAL_LOCAL
        && !type_wide((vsp - 1)->type))
        force_reg(vsp - 1);
    if (conversion_from >= 0 && out_here() == conversion_to
        && conversion_epoch == out_rewinds
        && (vsp - 1)->kind == VAL_REG && (vsp - 1)->val == R_HL) {
        out_rewind(conversion_from);
        jumps_forget(conversion_from);
    }
    conversion_from = -1;
    vdrop();
}

void vneg(void)
{
    Value *top = vsp - 1;
    int right;
    Type result;

    /* A wide constant is negated here: a float by its sign bit, which is
     * exact for zero as well, and an integer by two's complement at its own
     * width. */
    if (top->kind == VAL_WIDE) {
        uint64_t bits = wide_value(top);
        Type type = top->type;

        bits = type_float(type) ? bits ^ 0x80000000u : 0 - bits;
        if (type_wide_bytes(type) == 4)
            bits &= 0xffffffffu;
        vdrop();
        if (!wide_push(bits, type, 0))
            wide_to_slot(bits, type);

        return;
    }

    if (type_float(top->type)) {
        /* Negating a float is its sign bit flipped and nothing else -- no
         * routine, and correct for zero and for every other value alike. */
        int slot = long_scratch(TY_FLOAT);

        save_regs_below(1);
        materialise_long(slot, top->type);
        vdrop();

        ld_a_ix(slot + ACC_LONG_SIZE - 1);
        out_byte2(0xee, 0x80);          /* xor a, 0x80 */
        ld_ix_a(slot + ACC_LONG_SIZE - 1);
        vpush_scratch(top->type, slot);

        return;
    }

    if (type_wide(top->type)) {
        vunary_long(type_eight(top->type) ? RT_LLNEG : RT_LNEG, top->type);

        return;
    }

    if (val_const(top->kind) && !val_pending(top->kind)) {
        top->val = trunc_int(-top->val);

        return;
    }
    if (val_pending(top->kind)) {
        if (!in_function)
            no_addr_arithmetic();
        force_reg(top);
    }

    /* 0 - x, so the operand goes anywhere but HL and HL is then cleared of
     * whatever else was in it -- the zero is about to overwrite it. */
    if (!(top->kind == VAL_REG && top->val != R_HL))
        force_into(top, reg_alloc_other(R_HL));
    right = top->val;
    result = type_promote(top->type);
    evict_reg(R_HL);

    ld_rr_imm(R_HL, 0);
    or_a_a();
    sbc_hl_rr(right);
    vdrop();

    /* -x has the promoted type of x (C99 6.5.3.3), so the negation of an
     * unsigned is unsigned: pushed as a plain int, `-u >> 9` shifted in the
     * sign and came out as all ones. */
    vpush_reg(R_HL);
    (vsp - 1)->type = result;
}

void vnot(void)
{
    Value *top = vsp - 1;

    /* A long has its own routine rather than going round through -x - 1:
     * that composition would need a long subtract as well, and complementing
     * four bytes is four instructions. */
    /* ~ takes an integer. C makes a float operand a constraint violation
     * rather than something to define, so it is named rather than refused as
     * unfinished work. */
    if (type_float(top->type))
        acc_error_at(tok_line, "'~' takes an integer, not a floating-point value");

    if (top->kind == VAL_WIDE) {
        uint64_t bits = ~wide_value(top);
        Type type = top->type;

        if (type_wide_bytes(type) == 4)
            bits &= 0xffffffffu;
        vdrop();
        if (!wide_push(bits, type, 0))
            wide_to_slot(bits, type);

        return;
    }

    if (type_wide(top->type)) {
        vunary_long(type_eight(top->type) ? RT_LLNOT : RT_LNOT, top->type);

        return;
    }

    /* ~x is -x - 1, which needs no instruction this chip does not have. */
    vneg();
    vpush_const(1, TY_INT);
    vbinop(TK_MINUS);
}

/* ------------------------------------------------------------------ */
/* arithmetic at byte and short width                                  */

/* C promotes anything narrower than int before operating on it, so acc widens
 * every narrow load and narrows every narrow store. That is correct and, on
 * this chip, expensive: a byte load becomes `or a,a` / `sbc hl,hl` /
 * `ld l,(ix+d)`, getting the second operand into BC goes through the stack
 * because `sbc hl,hl` only works in HL, and `&` becomes a call into a helper
 * that pushes both operands to reach the byte of HL that has no name.
 *
 * Measured, a six-operation function in unsigned char came to 289 bytes
 * against 208 for the same in int -- narrow types cost more than wide ones,
 * on a machine whose ALU is eight bits wide.
 *
 * They need not. `add a,r` and `and a,r` are one cycle each in UM0077, the
 * same as `add hl,rr`, and `add a,(ix+d)` is four -- so `t = t + b` is three
 * instructions rather than the dozen promotion costs.
 *
 * The rule that makes it legal: promote-then-truncate and truncate-as-you-go
 * agree for + - & | ^ << and >>, because arithmetic modulo 2^8 is a ring
 * homomorphism. So computing in eight bits is indistinguishable from C
 * exactly when the result is narrowed back to that width and nothing
 * downstream sees the wider value. The parser decides that; this only emits.
 */

void ld_a_imm(int value)  { out_byte2(0x3e, value & 0xff); }
static void ld_a_ix_b(int disp)  { frame_byte(0x7e, disp); }

/* The A-with-memory and A-with-immediate forms, by token. */
static int alu_ix_op(int op)
{
    switch (op) {
    case TK_PLUS:  return 0x86;         /* add a,(ix+d) */
    case TK_MINUS: return 0x96;         /* sub a,(ix+d) */
    case TK_AMP:   return 0xa6;         /* and a,(ix+d) */
    case TK_PIPE:  return 0xb6;         /* or  a,(ix+d) */
    case TK_CARET: return 0xae;         /* xor a,(ix+d) */
    }

    return 0;
}

static int alu_imm_op(int op)
{
    switch (op) {
    case TK_PLUS:  return 0xc6;
    case TK_MINUS: return 0xd6;
    case TK_AMP:   return 0xe6;
    case TK_PIPE:  return 0xf6;
    case TK_CARET: return 0xee;
    }

    return 0;
}

/* A shift of A by one, which is what a constant count is unrolled into. */
static void shift_a_once(int op, Type to)
{
    out_byte(0xcb);
    if (op == TK_SHL)
        out_byte(0x27);                 /* sla a */
    else
        out_byte(type_unsigned(to) ? 0x3f : 0x2f);   /* srl a : sra a */
}

/* Is this value one the byte path can take as an operand? */
static int narrow_operand(const Value *val, Type to, int as_left)
{
    if (val_pending(val->kind))
        return 0;         /* masked into a byte it would no longer be one */
    if (val_const(val->kind))
        return 1;                       /* any constant; it is masked in */
    if (val->kind == VAL_ACC)
        return as_left;                 /* A is the accumulator, not a source */
    if (val->kind == VAL_LOCAL)
        return val->type == to;         /* same width and signedness */

    return 0;
}

static int vnarrow_ready(int op, Type to)
{
    const Value *lhs = vsp - 2;
    const Value *rhs = vsp - 1;

    /* Bytes only, and deliberately.
     *
     * A short is wider than A, so this path would have to become sixteen-bit
     * arithmetic -- and sixteen bits is the worst width on this chip. ADL mode
     * has no truncation logic in its 24-bit paths, so every sixteen-bit
     * operation masks or extends the upper byte, and reaching it means the
     * .SIS prefix and a mode switch. A short is better done at 24 bits and
     * truncated when it is stored, which is what the ordinary path already
     * does: acc emits no size-mode prefix anywhere.
     *
     * Measured, for the same six-operation function: 207 bytes of code as
     * unsigned char, 277 as unsigned int, 406 as unsigned short. The short is
     * dearer than the int, and all of that difference is the widening on load
     * and the truncation on store, which C requires for a two-byte object.
     * None of it is arithmetic this could make cheaper. */
    if ((unsigned) vtop < 2 || type_size(to) != 1)
        return 0;
    if (tok_pair(op, TK_SHL)) {
        /* Only a constant count, unrolled. A variable one is a loop, which is
         * what the helper already is. */
        if (!val_number(rhs->kind) || rhs->val < 0 || rhs->val > 8)
            return 0;
        return narrow_operand(lhs, to, 1);
    }
    if (!alu_ix_op(op))
        return 0;

    return narrow_operand(lhs, to, 1) && narrow_operand(rhs, to, 0);
}

static void vbinop_narrow(int op, Type to)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;

    /* The left operand into A, unless it is already there. */
    if (val_const(lhs->kind))
        ld_a_imm(lhs->val);
    else if (lhs->kind == VAL_LOCAL)
        ld_a_ix_b(lhs->val);

    if (tok_pair(op, TK_SHL)) {
        int count = rhs->val;

        while (count-- > 0)
            shift_a_once(op, to);
    } else if (val_const(rhs->kind)) {
        out_byte(alu_imm_op(op));
        out_byte(rhs->val & 0xff);
    } else {
        out_byte(0xdd);
        out_byte(alu_ix_op(op));
        out_byte(rhs->val);
    }

    vdrop();
    vdrop();
    vpush(VAL_ACC, to, 0);
}
/* ------------------------------------------------------------------ */
/* comparisons                                                         */

/* `sbc hl, rr` leaves the flags a comparison needs, but not in a form the
 * chip can branch on directly for a signed one.
 *
 * Z answers == and != on its own. Signed ordering does not: after a - b the
 * sign flag is the answer only when the subtraction did not overflow, and
 * when it did the answer is the opposite. That is the condition S xor V, and
 * there is no flag holding it and no instruction producing it, so the two
 * cases are branched apart. agondev's compiler calls a helper, __setflag, to
 * repair the flags; this does it in line because there is nothing to link
 * against.
 *
 * Only two shapes are emitted. `a > b` is `b < a` and `a <= b` is `b >= a`,
 * both got by swapping the operands before the registers are chosen, which
 * costs nothing: at that point the two are still descriptions on a stack. */

/* The comparison last emitted, and the bytes it spent turning the flags into
 * a one or a zero.
 *
 * Most of the time that value is what was wanted. In `if`, `while`, `for`,
 * `&&`, `||` and `?:` it is wanted only to be tested again against zero --
 * and the flags the subtract left already say the same thing. So vcmp leaves
 * a mark, and a branch that comes straight after it rewinds those bytes and
 * jumps on the flags instead. It is worth nineteen bytes a comparison, which
 * on a real program is a fifth of the image.
 *
 * "Straight after" is read off the image rather than tracked: if anything at
 * all has been emitted since, out_here() has moved and the mark says nothing.
 * The value stack is asked too, so that the answer being still where vcmp
 * put it is part of the bargain. */
int cmp_from = -1;       /* where the making of the value began */
int cmp_to;              /* and where it ended */
int cmp_op;              /* the comparison it was */
int cmp_was_unsigned;
unsigned cmp_epoch;       /* out_rewinds when it was made */

/* The same mark for an AND with a mask that keeps one byte of the value and
 * nothing else: the flags the AND left say whether that byte, and so the
 * whole answer, is zero -- until the sbc hl, hl that clears the rest, which
 * is where `from` stands. `if (c & 0x8000)` then comes to the AND and a
 * jump on its zero flag. */
static void flags_say_nonzero(int from)
{
    cmp_from = from;
    cmp_to = out_here();
    cmp_epoch = out_rewinds;
    cmp_op = TK_NE;
    cmp_was_unsigned = 1;
}


/* The equality half: Z is the whole answer. `when_equal` is what to leave
 * when the two were equal, which is 1 for `==` and 0 for `!=`. */
void cmp_equal(int when_equal)
{
    int to_done;

    ld_rr_imm(R_HL, when_equal);
    to_done = jump_op(JP_Z);
    ld_rr_imm(R_HL, 1 - when_equal);
    patch_to_here(to_done);
}

/* The unsigned ordering half, which needs none of the repair below: after
 * a - b the carry is set exactly when a was the smaller, whatever the two
 * were. `when_borrow` is 1 for `<` and 0 for `>=`. */
void cmp_unsigned(int when_borrow)
{
    int to_done;

    ld_rr_imm(R_HL, when_borrow);
    to_done = jump_op(JP_C);
    ld_rr_imm(R_HL, 1 - when_borrow);
    patch_to_here(to_done);
}

/* The signed ordering half. `when_negative` is what a truly negative difference
 * means: 1 for `<` and 0 for `>=`.
 *
 *      ld hl, when_negative
 *      jp pe, overflowed     ; the sign flag is not to be believed
 *      jp m,  done           ; it is, and the difference is negative
 *      jp     otherwise
 *  overflowed:
 *      jp p,  done           ; sign clear after an overflow means negative
 *  otherwise:
 *      ld hl, 1 - when_negative
 *  done:
 */
void cmp_signed(int when_negative)
{
    int to_overflowed, to_done, to_otherwise, to_done_from_overflow;

    ld_rr_imm(R_HL, when_negative);
    to_overflowed = jump_op(JP_PE);
    to_done       = jump_op(JP_M);
    to_otherwise  = jump_op(JP_ANY);

    patch_to_here(to_overflowed);
    to_done_from_overflow = jump_op(JP_P);

    patch_to_here(to_otherwise);
    ld_rr_imm(R_HL, 1 - when_negative);

    patch_to_here(to_done);
    patch_to_here(to_done_from_overflow);
}

/* A byte -- a char local, or one just read and widened -- against a
 * constant in its range, compared in A: `*p == '\n'` is ld a, (hl) and
 * cp 10, not the byte widened into HL, 10 loaded into DE and the two
 * subtracted. A signed byte is ordered as an unsigned one with its top bit
 * flipped, which keeps the order, so the carry says `<` for either.
 * `a > c` is `a >= c + 1` and `a <= c` is `a < c + 1`, so a constant one
 * past the top of the range is not taken for those. Returns 0 if the two
 * are not such a pair, having emitted nothing. */
static int cmp_byte_const(int op)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;
    Type type;
    int c, lo;

    if (val_number(lhs->kind)) {
        lhs = vsp - 1;                  /* 10 == c is c == 10 */
        rhs = vsp - 2;
        if (op == TK_LT || op == TK_LE)
            op = op == TK_LT ? TK_GT : TK_GE;
        else if (op == TK_GT || op == TK_GE)
            op = op == TK_GT ? TK_LT : TK_LE;
    }
    if (!val_number(rhs->kind))
        return 0;
    if (lhs->kind == VAL_LOCAL && !lhs->bits && type_size(lhs->type) == 1)
        type = lhs->type;
    else if ((type = widen_held(lhs)) == TY_VOID)
        return 0;

    c = rhs->val;
    if (tok_pair(op, TK_GT)) {
        c++;
        op = op == TK_GT ? TK_GE : TK_LT;
    }
    lo = type_unsigned(type) ? 0 : -128;
    if (c < lo || c > lo + 255)
        return 0;

    if (lhs->kind == VAL_LOCAL) {
        evict_reg(R_HL);                /* the answer is made there */
        ld_a_ix(lhs->val);
    } else {
        widen_undo(lhs);
    }
    if (!type_unsigned(type) && op != TK_EQ && op != TK_NE) {
        xor_a_imm(0x80);
        c ^= 0x80;
    }
    if (c & 0xff)
        cp_a_imm(c);
    else
        or_a_a();               /* the same Z and carry, a byte shorter */

    vdrop();
    vdrop();
    cmp_value(op, 1);

    return 1;
}

/* Z set when HL is zero, and HL as it was.
 *
 * There is no "is this register zero" instruction for a 24-bit value. The
 * upper byte of HL is not addressable, so the 16-bit idiom -- `ld a,l` then
 * `or a,h` -- would test two thirds of the value and call 0x010000 false.
 * Adding BC and taking it away again, with the carry cleared between, gives
 * back HL whatever BC holds, and the flags of a result that is HL: four
 * bytes, where loading a zero to subtract was six and wanted a register. */
void hl_zero_test(void)
{
    add_hl_rr(R_BC);
    or_a_a();
    sbc_hl_rr(R_BC);
}

/* See vcmp. Returns 0 when the constant is at the end of the range, where
 * c + 1 does not fit, having emitted nothing. */
static int signed_as_unsigned(int op)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;
    int rr;

    if (val_number(rhs->kind)) {
        int c = rhs->val;

        if (tok_pair(op, TK_GT)) {
            if (c >= 0x7fffff)
                return 0;
            c++;
            op = op == TK_GT ? TK_GE : TK_LT;
        }
        force_into(lhs, R_HL);
        /* Against 0 the sign is the answer: add hl, hl puts it in the
         * carry, a byte where moving both sides was twelve. */
        if (!c) {
            add_hl_rr(R_HL);
            vdrop();
            vdrop();
            cmp_value(op, 1);

            return 1;
        }
        rr = reg_busy(R_BC) && !reg_busy(R_DE) ? R_DE : R_BC;
        evict_reg(rr);
        ld_rr_imm(rr, 0x800000);
        add_hl_rr(rr);
        ld_rr_imm(rr, (c + 0x800000) & 0xffffff);
        or_a_a();
        sbc_hl_rr(rr);
    } else {
        if (tok_pair(op, TK_GT)) {
            Value swapped = *lhs;       /* a > b is b < a */

            *lhs = *rhs;
            *rhs = swapped;
            op = op == TK_GT ? TK_LT : TK_GE;
        }
        /* The right side where it is, in DE or BC, and nothing moved:
         * the difference's sign into the carry, and the carry turned over
         * where the subtract overflowed -- add hl, hl leaves P/V as sbc
         * set it. Six bytes after the subtract, and no other register,
         * where moving both by 0x800000 took the other of DE and BC. */
        int over;

        force_into(lhs, R_HL);
        rr = force_reg(rhs);
        or_a_a();
        sbc_hl_rr(rr);
        add_hl_rr(R_HL);
        over = jump_op(0xe2);           /* jp po */
        out_byte(0x3f);                 /* ccf */
        patch_to_here(over);
    }
    vdrop();
    vdrop();
    cmp_value(op, 1);

    return 1;
}

static void vcmp(int op)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;
    int folded, right, is_unsigned;

    if ((unsigned) vtop < 2)
        acc_error("internal: a comparison with nothing to compare");

    is_unsigned = cmp_is_unsigned(lhs, rhs);

    /* An unsigned comparison folds too, as long as neither side has its top
     * bit set: below that the two orderings are the same one, and above it
     * they are not. Which is what lets `sizeof x == 3` be a constant, since
     * a sizeof is unsigned and every size a program has is small. */
    if (val_const(lhs->kind) && val_const(rhs->kind)
        && foldable(op, lhs, rhs)
        && (!is_unsigned || (lhs->val >= 0 && rhs->val >= 0))
        && const_fold(op, lhs->val, rhs->val, &folded, is_unsigned)) {
        vdrop();
        vdrop();
        vpush_const(folded, TY_INT);    /* a comparison is an int either way */

        return;
    }

    if (cmp_byte_const(op))
        return;

    /* A signed order as an unsigned one, the carry the answer: against a
     * constant, 0x800000 added to both sides, which moves -8388608 to 0
     * and 8388607 to the top and keeps them in order -- the constant moved
     * already, and x > c as x >= c + 1 so that x stays in HL; otherwise the
     * sign of the difference turned into the carry. The signed answer read
     * the sign and the overflow, which is three jumps and fourteen bytes
     * where the carry is one jump. */
    if (!is_unsigned && op != TK_EQ && op != TK_NE
        && !val_const(lhs->kind) && signed_as_unsigned(op))
        return;

    /* Equal to zero, or not: HL tested where it is. */
    if ((op == TK_EQ || op == TK_NE) && val_number(rhs->kind) && rhs->val == 0
        && !val_const(lhs->kind)) {
        force_into(lhs, R_HL);
        hl_zero_test();
        vdrop();
        vdrop();
        cmp_value(op, is_unsigned);

        return;
    }

    /* `a > b` is `b < a`, and `a <= b` is `b >= a`. Swapping costs nothing
     * here: both sides are still descriptions on a stack, not registers. */
    if (tok_pair(op, TK_GT)) {
        Value swapped = *lhs;

        *lhs = *rhs;
        *rhs = swapped;
        op = (op == TK_GT) ? TK_LT : TK_GE;
    }

    force_into(vsp - 2, R_HL);
    right = force_reg(vsp - 1);

    or_a_a();                   /* sbc reads the carry, so clear it */
    sbc_hl_rr(right);

    vdrop();
    vdrop();

    cmp_value(op, is_unsigned);
}

/* HL = 1 or 0 from the flags a comparison left, and the mark that lets a
 * branch undo it. */
void cmp_value(int op, int is_unsigned)
{
    cmp_from = out_here();
    cmp_op = op;
    cmp_was_unsigned = is_unsigned;

    switch (op) {
    case TK_EQ: cmp_equal(1); break;
    case TK_NE: cmp_equal(0); break;
    case TK_LT: if (is_unsigned) cmp_unsigned(1); else cmp_signed(1); break;
    case TK_GE: if (is_unsigned) cmp_unsigned(0); else cmp_signed(0); break;
    default:
        acc_error("internal: %s is not a comparison", tok_spelling(op));
    }

    cmp_to = out_here();
    cmp_epoch = out_rewinds;
    vpush_reg(R_HL);
}

/* The type two operands meet at when either is four bytes wide: C's usual
 * arithmetic conversions, for the widths this machine has.
 *
 * Floating wins over any integer. Between integers the long wins, and it is
 * unsigned only if one of the operands was itself an unsigned long -- not, as
 * acc had it, if either operand was unsigned at all. An unsigned int here is
 * twenty-four bits, and a signed long of thirty-two holds every one of them,
 * so C converts the pair to long: `(unsigned) 1 + -2L` is -1, and is less
 * than zero. Making it unsigned long made it 4294967295. */
Type common_wide(Type left, Type right)
{
    if (type_float(left) || type_float(right))
        return TY_FLOAT;

    /* A long long holds every long and every unsigned long, so it is signed
     * unless one of the two was an unsigned long long. */
    if (type_eight(left) || type_eight(right))
        return (type_eight(left) && type_unsigned(left))
               || (type_eight(right) && type_unsigned(right))
               ? TY_ULLONG : TY_LLONG;
    if ((type_wide(left) && type_unsigned(left))
        || (type_wide(right) && type_unsigned(right)))
        return TY_ULONG;

    return TY_LONG;
}

/* The top two values exchanged. Nothing is emitted: a Value says where a
 * value is, not where it is on this stack, so swapping two of them is a
 * swap of two descriptors. */
void vswap(void)
{
    Value held = *(vsp - 1);

    *(vsp - 1) = *(vsp - 2);
    *(vsp - 2) = held;
}

/* The operators that leave a 0 or 1 behind rather than a number. */
static int is_comparison(int op)
{
    switch (op) {
    case TK_EQ: case TK_NE:
    case TK_LT: case TK_GT: case TK_LE: case TK_GE:
        return 1;
    }

    return 0;
}

/* Every binary operator in the program comes through here.
 *
 * The parser used to make this decision itself, which cost it a call to ask
 * whether either side was a long, two more to fetch the types it would need
 * if one was, and a fourth to ask whether the narrow path could take it --
 * four calls across the file boundary before a byte was emitted, for an
 * answer that is entirely in the top two values of a stack the parser cannot
 * see. Asking once and deciding here is the same decision made where the
 * facts are, and the pieces of it fold into each other because they are no
 * longer separately reachable.
 */
void vapply(unsigned char op, Type narrow)
{
    Type left, right;

    if ((unsigned) vtop < 2)
        acc_error("internal: an operator with nothing to apply it to");

    left  = (vsp - 2)->type;
    right = (vsp - 1)->type;

    /* `x != 0` and `x == 0` straight after a comparison or an AND that left
     * its mark are `!!x` and `!x`: vtruth reads the flags. `(c & 1) != 0`
     * is how a test of a bit is written, and is every one of zap's
     * character classes. */
    if ((op == TK_EQ || op == TK_NE) && (vsp - 1)->kind == VAL_CONST
        && (vsp - 1)->val == 0 && cmp_from >= 0 && out_here() == cmp_to && cmp_epoch == out_rewinds
        && (vsp - 2)->kind == VAL_REG && (vsp - 2)->val == R_HL) {
        vdrop();
        vtruth(op);

        return;
    }

    /* And of a value in HL whose upper bytes are known to be zero: the rest
     * are tested in A, and the flags that leaves are a mark like the AND's. */
    if ((op == TK_EQ || op == TK_NE) && (vsp - 1)->kind == VAL_CONST
        && (vsp - 1)->val == 0 && (vsp - 2)->kind == VAL_REG
        && (vsp - 2)->val == R_HL && vwidth(vsp - 2) < 3) {
        int width = vwidth(vsp - 2);

        vdrop();
        if (width == 2 || !widen_undo(vsp - 1))
            ld_a_l();           /* unless the byte is in A, unwidened */
        if (width == 2)
            out_byte(0xb4);                     /* or h */
        else
            or_a_a();
        flags_say_nonzero(out_here());
        vtruth(op);

        return;
    }

    if (type_pointer(left) || type_pointer(right)) {
        if (!is_comparison(op)) {
            vbinop_pointer(op, left, right);

            return;
        }
        vcmp_pointer_check(left, right);
    }

    /* A shift is not one of the operators the usual arithmetic conversions
     * apply to. C99 6.5.7 promotes each operand on its own and gives the
     * result the type of the promoted left one, so a long count does not
     * make the shift a long shift, and an int shifted by a long stays an
     * int: `(unsigned) 0x400000 << 2L` has to wrap at 24 bits like any other
     * unsigned int, and acc gave 16777216 for it while agondev gave 0.
     *
     * Which also settles what the runtime should fill with on a right
     * shift -- the sign of the left operand, never the count's. */
    if (tok_pair(op, TK_SHL)) {
        if (type_wide(left)) {
            vbinop_long(op, left);

            return;
        }

        /* The count comes down to a width a register holds. Only its low
         * bits can matter: anything from the width up is undefined. */
        if (type_wide(right)) {
            vconvert(TY_INT);
            right = (vsp - 1)->type;
        }
    }

    /* Either side a long or a float makes both of them one, and the result
     * is that -- or, for a comparison, an int taken from a comparison at that
     * width. */
    if (type_wide(left) || type_wide(right)) {
        Type wide = common_wide(left, right);

        /* An integer operand of a floating operator is converted to float
         * first, which is arithmetic and not a relabelling. Without it, a
         * float and an int could not meet at all: `f + 1` was refused as a
         * conversion between the two that was not implemented. Both sides are
         * reached by swapping, since only the top of the stack converts. */
        if (type_float(wide)) {
            if (!type_float(right))
                vconvert(TY_FLOAT);
            if (!type_float(left)) {
                vswap();
                vconvert(TY_FLOAT);
                vswap();
            }
        }

        if (is_comparison(op))
            vcmp_wide(op, wide);
        else
            vbinop_long(op, wide);

        return;
    }

    if (is_comparison(op)) {
        vcmp(op);

        return;
    }

    if (narrow && vnarrow_ready(op, narrow)) {
        vbinop_narrow(op, narrow);

        return;
    }

    vbinop(op);
}

#ifdef OPT_ACC
/* This file's marks -- where a sequence just emitted ended, and the
 * out_rewinds it was made at -- copied out, or back, for genlog.c, which
 * compiles a function again from where it began and needs every mark as it
 * stood then. Returns how many bytes they take. */
size_t arith_marks(unsigned char *buf, int restore)
{
    size_t at = 0;

    STATE_VAR(gaddr_end);
    STATE_VAR(gaddr_reg);
    STATE_VAR(gaddr_epoch);
    STATE_VAR(conversion_from);
    STATE_VAR(conversion_to);
    STATE_VAR(conversion_epoch);
    STATE_VAR(cmp_from);
    STATE_VAR(cmp_to);
    STATE_VAR(cmp_op);
    STATE_VAR(cmp_was_unsigned);
    STATE_VAR(cmp_epoch);

    return at;
}
#endif
