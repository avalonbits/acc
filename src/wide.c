/*
 * long, long long and float: values wider than a register, the calls into
 * the runtime that do their arithmetic, converting between them and ints,
 * and the pool a function's wide constants are kept in.
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

static void wide_bytes_at(int disp, uint64_t bits, int n);

/* lea rr, ix+d -- the address of a frame slot, which is what the helpers take.
 * The second byte is the register, and these are the assembler's own numbers
 * rather than a reading of the opcode map: guessing DE cost a debugging pass. */
void lea_rr_ix(int reg, int disp)
{
    static const unsigned char lea_code[NREGS] = { 0x22, 0x12, 0x02 };

    if (disp_fits(disp))
        out_byte3(0xed, lea_code[reg], disp);
    else
        out_byte3(0xed, lea_code[reg] + 1, far_base(disp));  /* from iy */
}

/* Copy n bytes from one frame slot to another.
 *
 * The two instructions are laid down here rather than through ld_a_ix and
 * ld_ix_a, which are calls: out_byte3 is inlined, so a byte of the copy is
 * six stores behind one bounds check instead of two calls. Worth the
 * departure from how the rest of the file emits, because this is the loop
 * that runs for every byte of every wide value a program moves -- it was
 * 4.8% of a compile of the long benchmark. */
static void copy_long(int to, int from, int n)
{
    int i;

    if (!disp_fits(from) || !disp_fits(from + n - 1)
        || !disp_fits(to) || !disp_fits(to + n - 1)) {
        for (i = 0; i != n; i++) {               /* one of them is out of reach */
            ld_a_ix(from + i);
            ld_ix_a(to + i);
        }

        return;
    }

    /* Three bytes at a time through IY, the backend's scratch, where the
     * two slots are apart: a long is two of those, the second overlapping
     * the first by two bytes, which writes them again with what they
     * already hold -- twelve bytes of code where a byte at a time was
     * twenty-four. */
    if (n >= ACC_INT_SIZE && (to + n <= from || from + n <= to)) {
        for (i = 0; i < n; i += ACC_INT_SIZE) {
            if (i > n - ACC_INT_SIZE)
                i = n - ACC_INT_SIZE;
            out_byte3(0xdd, 0x31, from + i);    /* ld iy, (ix+d) */
            out_byte3(0xdd, 0x3e, to + i);      /* ld (ix+d), iy */
        }

        return;
    }
    for (i = 0; i != n; i++) {
        out_byte3(0xdd, 0x7e, from + i);        /* ld a, (ix+d) */
        out_byte3(0xdd, 0x77, to + i);          /* ld (ix+d), a */
    }
}

/* The bytes of a slot from `from` to `to` set to A: the extension of a
 * value that is narrower than the slot. */
static void fill_from_a(int disp, int from, int to)
{
    for (; from < to; from++)
        ld_ix_a(disp + from);
}

/* Write an int-wide value, already in HL, into a slot of n bytes: three
 * bytes and then the ones the sign or the zero extension calls for. */
static void store_int_as_long(int disp, int is_unsigned, int n)
{
    ld_ix_rr(disp, R_HL);
    if (is_unsigned) {
        out_byte(0xaf);                 /* xor a, a */
    } else {
        /* The sign of a 24-bit value is bit 23, which has no name -- but
         * `add hl, hl` shifts it into the carry, and a push and a pop either
         * side leave HL as it was. */
        push_rr(R_HL);
        add_hl_rr(R_HL);
        pop_rr(R_HL);
        out_byte(0x9f);                 /* sbc a, a: 0 or 0xff */
    }
    fill_from_a(disp, ACC_INT_SIZE, n);
}

/* An integer becoming a float, and a float becoming an integer. The bytes
 * mean different things, so this is arithmetic and not a relabelling.
 *
 * Both go through an int: the narrow types widen to one on the way in, and a
 * long is not handled here because thirty-two bits do not fit in a float's
 * twenty-four of significand without rounding, and there is no routine for
 * that rounding yet.
 *
 * save_regs_below(1) for the reason the long operators have it: the lea
 * loads a register with an address behind the allocator's back. */
void convert_int_to_float(void)
{
    Value *top = vsp - 1;
    int unsign = type_unsigned(top->type) && type_size(top->type) >= ACC_INT_SIZE;
    int slot;

    save_regs_below(1);

    /* A long is already four bytes in the frame and the routine rewrites it
     * where it lies, which is what the caller wants: a long and a float are
     * the same width and want the same kind of slot. */
    if (type_wide(top->type)) {
        int eight = type_eight(top->type);

        slot = long_scratch(top->type);
        materialise_long(slot, top->type);
        vdrop();

        lea_rr_ix(R_HL, slot);
        if (eight)
            rt_call(unsign ? RT_ULLTOF : RT_LLTOF);
        else
            rt_call(unsign ? RT_ULTOF : RT_LTOF);
        vpush_scratch(TY_FLOAT, slot);

        return;
    }

    force_into(top, R_HL);

    slot = long_scratch(TY_FLOAT);
    lea_rr_ix(R_DE, slot);
    rt_call(unsign ? RT_UITOF : RT_ITOF);
    vdrop();
    vpush_scratch(TY_FLOAT, slot);
}

void convert_float_to_int(Type to)
{
    int slot;

    save_regs_below(1);

    /* The routine takes an address, so a float that is not already in the
     * frame has to be put there. Every float is, as it happens -- four bytes
     * do not fit in a register -- but materialise_long is what says so. A
     * long long's slot is eight bytes, the float in the first four. */
    slot = type_eight(to) ? long_scratch(to) : long_scratch(TY_FLOAT);
    materialise_long(slot, TY_FLOAT);
    vdrop();

    lea_rr_ix(R_HL, slot);

    /* A long stays in the frame, where the routine rewrites it in place. */
    if (type_wide(to)) {
        rt_call(type_eight(to) ? RT_FTOLL : RT_FTOL);
        vpush_scratch(to, slot);

        return;
    }

    rt_call(RT_FTOI);
    vpush_reg(R_HL);
    (vsp - 1)->type = TY_INT;

    /* And then down to whatever narrow type was asked for, which is the
     * ordinary integer conversion and not this one. */
    if (type_size(to) < ACC_INT_SIZE)
        vconvert(to);
    else
        (vsp - 1)->type = to;
}

/* An address moves with the image; the floating-point value made from one
 * cannot, because its bytes are no longer an address for a relocation to
 * name. Only the two paths that reach a float from an integer ask, so the
 * conversion every argument of every call goes through does not. */
void no_float_address(const Value *from)
{
    if (val_pending(from->kind))
        acc_error_at(tok_line, "an address cannot become a floating-point "
                               "value");
}

/* The bytes of a float and of an integer mean different things, so moving a
 * value between them is arithmetic and not a copy. Every path that widens or
 * stores four bytes comes through here, which is why the check lives here
 * rather than at each of them. */
void check_no_float_mix(Type to, const Value *from)
{
    if (type_float(to) == type_float(from->type))
        return;
    if (val_number(from->kind) && from->val == 0)
        return;                 /* zero is all zero bits either way */

    acc_error_at(tok_line, "converting between floating-point and integer is "
                           "not implemented yet");
}

/* Put the top of the stack into a wide slot, whatever width it arrived as:
 * a long into a long long's slot is extended by its sign or by zero, and a
 * long long into a long's is its low four bytes. */
void materialise_long(int disp, Type type)
{
    Value *top = vsp - 1;
    int n = type_wide_bytes(type);

    check_no_float_mix(type, top);

    /* An address is the exception, and goes through a register.
     *
     * The relocation that moves it with the image covers three bytes in a
     * row, and a constant written the way below puts two instructions
     * between each byte and the next -- so there would be nothing for a
     * relocation to name. Loaded into a register and stored from it, the
     * three bytes are together in the instruction that loads it, and the
     * fourth is the zero every address has. */
    if (val_pending(top->kind)) {
        int reg = force_reg(top);
        int i;

        ld_ix_rr(disp, reg);
        for (i = ACC_INT_SIZE; i < n; i++) {
            ld_a_imm(0);
            ld_ix_a(disp + i);
        }

        return;
    }

    /* A constant goes in as its bytes, whatever width it came as: no load,
     * no sign extension, and for a long long no runtime call. */
    if (top->kind == VAL_WIDE || val_const(top->kind)) {
        wide_bytes_at(disp, const_as(top, type), n);

        return;
    }

    if (top->kind == VAL_LOCAL && type_wide(top->type)) {
        int from = type_wide_bytes(top->type);

        if (top->val == disp && from == n)
            return;                     /* already there */
        if (from >= n) {
            copy_long(disp, top->val, n);

            return;
        }
        copy_long(disp, top->val, from);
        if (type_unsigned(top->type)) {
            out_byte(0xaf);                     /* xor a, a */
        } else {
            ld_a_ix(top->val + from - 1);       /* the top byte, for its sign */
            out_byte2(0x87, 0x9f);              /* add a, a; sbc a, a */
        }
        fill_from_a(disp, from, n);

        return;
    }
    force_into(top, R_HL);
    store_int_as_long(disp, type_unsigned(top->type), n);
}

/* A floating constant, laid down as the four bytes the machine reads. The
 * host's float is the same IEEE 754 single this target uses, so the bits are
 * taken from it rather than assembled: anything else would be a second
 * implementation of the format, to be got wrong separately. */
void vpush_const_float(float val)
{
    uint32_t bits;

    memcpy(&bits, &val, sizeof bits);
    if (!wide_push(bits, TY_FLOAT, 0))
        wide_to_slot(bits, TY_FLOAT);
}

/* A constant too wide for a register goes straight to a frame slot, which is
 * where every long lives. `high` is the upper half of a long long's. */
void vpush_const_long(long val, Type type)
{
    vpush_const_wide((uint32_t) val, 0, type);
}

void vpush_const_wide(uint32_t low, uint32_t high, Type type)
{
    uint64_t bits = (uint64_t) high << 32 | low;

    if (!wide_push(bits, type, 0))
        wide_to_slot(bits, type);
}

/* n bytes between the frame slot at `slot` and where HL points, read from
 * there or written there: three at a time through IY where the slot is in
 * reach, ld iy, (hl) and a store, or the other way round, with the last
 * three overlapping the ones before if n is not a multiple of three -- a
 * long is twelve bytes of code where a byte at a time through A was
 * nineteen. HL is left past the start, wherever the last run began. */
void wide_through_hl(int slot, int n, int store)
{
    int i, at = 0;

    if (n >= ACC_INT_SIZE && disp_fits(slot) && disp_fits(slot + n - 1)) {
        for (i = 0; i < n; i += ACC_INT_SIZE) {
            if (i > n - ACC_INT_SIZE)
                i = n - ACC_INT_SIZE;
            while (at < i) {
                inc_hl();
                at++;
            }
            if (store) {
                out_byte3(0xdd, 0x31, slot + i);    /* ld iy, (ix+d) */
                out_byte2(0xed, 0x3e);              /* ld (hl), iy */
            } else {
                out_byte2(0xed, 0x31);              /* ld iy, (hl) */
                out_byte3(0xdd, 0x3e, slot + i);    /* ld (ix+d), iy */
            }
        }

        return;
    }
    for (i = 0; i != n; i++) {
        if (store) {
            ld_a_ix(slot + i);
            ld_hl_a();
        } else {
            ld_a_hl();
            ld_ix_a(slot + i);
        }
        if (i < n - 1)
            inc_hl();
    }
}

/* The bytes of a wide constant written into a frame slot: `ld a, n` and a
 * store, a byte at a time.
 *
 * The bytes are read out of `bits` where it lies, lowest first, as get24
 * reads a value: shifted out, each was a call to the runtime's 64-bit
 * shift, which goes a bit at a time -- over a thousand cycles a byte, and
 * an eighth of the time acc took over test/bench's numeric.c. */
static void wide_bytes_at(int disp, uint64_t bits, int n)
{
    int i;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    unsigned char bytes[8];

    for (i = 0; i != 8; i++)
        bytes[i] = (unsigned char) (bits >> (i * 8));
#else
    const unsigned char *bytes = (const unsigned char *) &bits;
#endif

    /* Three bytes at a time through IY where the slot is near: ld iy, nn
     * and a store is eight bytes against fifteen. The last run overlaps
     * the one before it rather than going a byte at a time. */
    if (n >= ACC_INT_SIZE && disp_fits(disp) && disp_fits(disp + n - 1)) {
        int last = -1;

        for (i = 0; i < n; i += ACC_INT_SIZE) {
            if (i > n - ACC_INT_SIZE)
                i = n - ACC_INT_SIZE;
            /* IY already holds these three bytes -- a zero's runs are all
             * alike -- so it is stored again without the load. */
            if (last < 0 || memcmp(bytes + last, bytes + i, 3)) {
                out_byte2(0xfd, 0x21);          /* ld iy, nn */
                out_byte3(bytes[i], bytes[i + 1], bytes[i + 2]);
            }
            out_byte3(0xdd, 0x3e, disp + i);    /* ld (ix+d), iy */
            last = i;
        }

        return;
    }
    for (i = 0; i != n; i++) {
        out_byte(0x3e);                         /* ld a, n */
        out_byte(bytes[i]);
        ld_ix_a(disp + i);
    }
}

/* And a slot of its own for it, which is what a wide constant becomes when
 * something needs it where a wide value lives. */
void wide_to_slot(uint64_t bits, Type type)
{
    int slot = spill_slot_of(type_wide_bytes(type));

    wide_bytes_at(slot, bits, type_wide_bytes(type));
    vpush_scratch(type, slot);
}

/* One scratch slot as wide as the type, for the left operand of an operation
 * to be built in and overwritten by the result. */
int long_scratch(Type type)
{
    return spill_slot_of(type_wide_bytes(type));
}

/* Scratch reused within a statement, which is what an eight-byte value needs.
 *
 * The slots are a high-water mark released at the end of the statement, and
 * that is all a four-byte long ever needed: two slots an operator, against a
 * frame that reaches 128 bytes. Eight-byte values run it out -- `a * 3LL !=
 * 3000000000000LL` takes forty bytes of scratch, and a handful of those is a
 * whole frame.
 *
 * What an operator can give back is the scratch its own operands are in: a
 * constant put in the frame for it, or the answer of the operator below it,
 * is dead the moment this one has read it. So the answer is built at the
 * lowest of those rather than on top of them, and the mark comes back to
 * just past it. Everything else the statement is holding stays where it is:
 * Nothing is built below where the values still on the stack end.
 *
 * A slot is described by where it starts in the scratch area, which is what
 * the displacement and the width say between them. */
int spill_start_of(const Value *v, int *size)
{
    int bytes = type_wide(v->type) ? type_wide_bytes(v->type)
                                   : type_scalar_bytes(type_promote(v->type));
    int start;

    if (v->kind != VAL_LOCAL || v->val >= -locals_size)
        return -1;                      /* a local of its own, not scratch */

    start = -v->val - locals_size - bytes;
    if (start < 0 || start + bytes > spill_peak)
        return -1;                      /* not the scratch area at all */
    *size = bytes;

    return start;
}

/* The lowest place this operator may build its answer.
 *
 * The scratch of the operands it consumes is its to take; everything else
 * the statement is still holding stays where it is, and the answer goes
 * above the highest of it. `live_top` says the top operand is not consumed
 * but read where it lies, which makes it one of the values to stay clear of
 * rather than one to take. */
static int spill_lowest(int operands, int live_top)
{
    int low = spill_used, floor = 0, first = vtop - operands, size, i;

    for (i = 0; i < vtop; i++) {
        int start = spill_start_of(vstack + i, &size);
        int consumed = i >= first && !(live_top && i == vtop - 1);

        if (start < 0)
            continue;
        if (!consumed) {
            if (start + size > floor)
                floor = start + size;
        } else if (start < low) {
            low = start;
        }
    }

    return low < floor ? floor : low;
}

/* Whether the routine writes through DE as well as HL, which says whether
 * its right operand has to be a copy.
 *
 * The divisions do: they leave the remainder where the divisor was. So does
 * the float subtract, which turns the right operand's sign over and adds --
 * and the float comparison, which rewrites both operands as the unsigned
 * integers that sort the way they do. All three are written that way on
 * purpose, because the operand was scratch the operator had finished with;
 * this list is what keeps that true. Everything else only reads. */
static int helper_writes_right(int which)
{
    switch (which) {
    case RT_LDIVU:  case RT_LREMU:  case RT_LDIVS:  case RT_LREMS:
    case RT_LLDIVU: case RT_LLREMU: case RT_LLDIVS: case RT_LLREMS:
    case RT_FSUB:   case RT_FCMP:
        return 1;
    }

    return 0;
}

/* Whether a value is already what the routine wants to read: in the frame,
 * of the width the operation is at, and meaning what its bytes say -- a long
 * and an unsigned long are the same four bytes, a float is not. A global is
 * not, either: it is read into scratch on the way, and this is asked of what
 * is on the stack by then. */
static int wide_in_place(const Value *v, Type type, int n)
{
    return v->kind == VAL_LOCAL && type_wide(v->type)
        && type_wide_bytes(v->type) == n
        && type_float(v->type) == type_float(type);
}

/* A slot at a given place in the scratch area, which may be one an operand
 * is still in: what materialise_long writes there is read from the same
 * place or from above it, never from below. */
static int slot_at(int start, int size)
{
    if (start + size > spill_peak)
        spill_peak = start + size;

    return -(locals_size + start + size);
}

/* Which routine applies an operator to two four-byte values. A float has its
 * own for everything, because the bytes mean something different: the integer
 * add walks them with a carry, which on a float is arithmetic on the bit
 * pattern and not on the number -- 3.0 + 1.0 done that way comes out as a
 * value with every exponent bit set. */
static int float_helper(int op)
{
    switch (op) {
    case TK_PLUS:  return RT_FADD;
    case TK_MINUS: return RT_FSUB;
    case TK_STAR:  return RT_FMUL;
    case TK_SLASH: return RT_FDIV;

    /* The rest are not missing, they are not allowed: C requires integer
     * operands for the remainder, the bitwise operators and the shifts, so a
     * float there is the program's mistake and not acc's shortfall. Saying
     * "not implemented yet" would send someone looking for a routine that
     * should never exist. */
    case TK_PERCENT:
    case TK_AMP: case TK_PIPE: case TK_CARET:
    case TK_SHL: case TK_SHR:
        acc_error_at(tok_line, "%s takes integers, not floating-point values",
                     tok_spelling(op));
    }

    return -1;
}

/* The long long routines, in the order of the operators' long ones. */
static int long_long_helper(int op, Type type)
{
    switch (op) {
    case TK_PLUS:  return RT_LLADD;
    case TK_MINUS: return RT_LLSUB;
    case TK_AMP:   return RT_LLAND;
    case TK_PIPE:  return RT_LLOR;
    case TK_CARET: return RT_LLXOR;
    case TK_STAR:  return RT_LLMUL;
    case TK_SHL:   return RT_LLSHL;
    case TK_SHR:   return type_unsigned(type) ? RT_LLSHRU : RT_LLSHRS;
    case TK_SLASH: return type_unsigned(type) ? RT_LLDIVU : RT_LLDIVS;
    case TK_PERCENT: return type_unsigned(type) ? RT_LLREMU : RT_LLREMS;
    }

    return -1;
}

static int long_helper(int op, Type type)
{
    if (type_float(type))
        return float_helper(op);
    if (type_eight(type))
        return long_long_helper(op, type);

    switch (op) {
    case TK_PLUS:  return RT_LADD;
    case TK_MINUS: return RT_LSUB;
    case TK_AMP:   return RT_LAND;
    case TK_PIPE:  return RT_LOR;
    case TK_CARET: return RT_LXOR;
    case TK_STAR:  return RT_LMUL;
    case TK_SHL:   return RT_LSHL;

    /* The ones that read the sign. A shift right fills with the sign for a
     * signed left operand and with zero for an unsigned one, and division
     * has to truncate towards zero, which the unsigned loop cannot do. */
    case TK_SHR:   return type_unsigned(type) ? RT_LSHRU : RT_LSHRS;
    case TK_SLASH: return type_unsigned(type) ? RT_LDIVU : RT_LDIVS;
    case TK_PERCENT: return type_unsigned(type) ? RT_LREMU : RT_LREMS;
    }

    return -1;
}

/* Two wide constants, worked out here rather than by the program.
 *
 * The width and the sign come from the type the operands met at, which is
 * what the runtime routine would have used: an eight-byte divide is not a
 * four-byte one with the top bytes ignored, and a signed shift right is not
 * an unsigned one. Division by zero is not folded -- C leaves it undefined,
 * the runtime answers zero, and the compiler must not be the thing that
 * divides by it. */
int64_t wide_signed(uint64_t bits, int width)
{
    if (width == 8)
        return (int64_t) bits;

    return (int32_t) bits;
}

static int fold_wide_int(int op, Type type, uint64_t a, uint64_t b,
                         uint64_t *out)
{
    int width = type_wide_bytes(type);
    int unsign = type_unsigned(type) != 0;
    uint64_t mask = width == 8 ? ~(uint64_t) 0 : 0xffffffffu;
    unsigned count = (unsigned) (b & (width == 8 ? 63 : 31));

    switch (op) {
    case TK_PLUS:  *out = a + b;  break;
    case TK_MINUS: *out = a - b;  break;
    case TK_STAR:  *out = a * b;  break;
    case TK_AMP:   *out = a & b;  break;
    case TK_PIPE:  *out = a | b;  break;
    case TK_CARET: *out = a ^ b;  break;
    case TK_SHL:   *out = a << count; break;
    case TK_SHR:
        *out = unsign ? (a & mask) >> count
                      : (uint64_t) (wide_signed(a, width) >> count);
        break;
    case TK_SLASH:
        if ((b & mask) == 0)
            return 0;
        *out = unsign ? (a & mask) / (b & mask)
                      : (uint64_t) (wide_signed(a, width)
                                    / wide_signed(b, width));
        break;
    case TK_PERCENT:
        if ((b & mask) == 0)
            return 0;
        *out = unsign ? (a & mask) % (b & mask)
                      : (uint64_t) (wide_signed(a, width)
                                    % wide_signed(b, width));
        break;
    default:
        return 0;
    }
    *out &= mask;

    return 1;
}

/* The same for two floats, in float.c's own arithmetic rather than in the
 * float of whichever compiler built this one: the host's and agondev's do
 * not agree at the edges, and a constant folded here has to come out the
 * same in both builds. Not for a division by zero, which is an infinity the
 * runtime routine makes and which C leaves to it. */
static int fold_wide_float(int op, uint64_t a, uint64_t b, uint64_t *out)
{
    uint32_t x = (uint32_t) a, y = (uint32_t) b, r;

    switch (op) {
    case TK_PLUS:  r = float_add(x, y); break;
    case TK_MINUS: r = float_add(x, float_neg(y)); break;
    case TK_STAR:  r = float_mul(x, y); break;
    case TK_SLASH:
        if (float_is_zero(y))
            return 0;
        r = float_div(x, y);
        break;
    default:
        return 0;
    }
    *out = r;

    return 1;
}

/* -x and ~x on a long, which are the same shape: the value goes to a scratch
 * slot and the routine works on it there. The 24-bit forms hold the value in
 * HL and cannot be reached for: a long does not fit in a register.
 *
 * save_regs_below(1) for the reason vbinop_long has it -- the lea loads HL
 * behind the register allocator's back, so anything else living in a register
 * has to come out first. The top is exempt: it is the operand. */
void vunary_long(int which, Type type)
{
    int slot;

    save_regs_below(1);

    slot = long_scratch(type);
    materialise_long(slot, type);
    vdrop();

    lea_rr_ix(R_HL, slot);
    rt_call(which);

    vpush_scratch(type, slot);
}
static uint64_t pool_val[POOL_MAX];
int      pool_n[POOL_MAX], pool_addr[POOL_MAX], npool;
int     *pool_site_at, *pool_site_entry, npool_sites, pool_sites_cap;

/* ld de or ld hl, the constant's address. The pool has room: whoever asks
 * has seen npool below POOL_MAX, and a constant already in it takes none. */
static void ld_rr_pool(int reg, uint64_t v, int n)
{
    int e;

    for (e = 0; e != npool; e++)
        if (pool_val[e] == v && pool_n[e] == n)
            break;
    if (e == npool) {
        if (npool == POOL_MAX)
            acc_error("internal: a function's constants overflowed");
        pool_val[npool] = v;
        pool_n[npool++] = n;
    }
    if (npool_sites == pool_sites_cap) {
        pool_sites_cap = pool_sites_cap ? pool_sites_cap * 2 : 16;
        pool_site_at = realloc(pool_site_at,
                               (size_t) pool_sites_cap * sizeof *pool_site_at);
        pool_site_entry = realloc(pool_site_entry, (size_t) pool_sites_cap
                                                   * sizeof *pool_site_entry);
        if (!pool_site_at || !pool_site_entry)
            acc_error("out of memory for a function's constants");
    }
    out_byte(reg == R_HL ? 0x21 : 0x11);        /* ld hl or de, nn */
    pool_site_at[npool_sites] = out_here();
    pool_site_entry[npool_sites++] = e;
    out_reloc(out_here());
    out_word24(0);
}

/* The pool, after the function's code and its jumps shortened, and each
 * use pointed at its constant. Only the constants still used: a rewind may
 * have taken back every use of one. */
void pool_emit(void)
{
    int e, i;

    for (e = 0; e != npool; e++)
        pool_addr[e] = -1;
    for (i = 0; i != npool_sites; i++) {
        e = pool_site_entry[i];
        if (pool_addr[e] < 0) {
            unsigned char b[8];

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            int j;

            for (j = 0; j != 8; j++)
                b[j] = (unsigned char) (pool_val[e] >> (j * 8));
#else
            memcpy(b, &pool_val[e], sizeof b);
#endif
            pool_addr[e] = out_here();
            for (int j2 = 0; j2 < pool_n[e]; j2++)
                out_byte(b[j2]);
        }
        out_patch24(pool_site_at[i], pool_addr[e]);
    }
    npool = npool_sites = 0;
}

/* A rewind to `here` takes back the uses of the pool at or past it. */
void gen_rewound(int here)
{
    const int *last;

    if (!npool_sites)
        return;                 /* nearly every rewind: nothing to look at */
    last = pool_site_at + npool_sites;
    while (last > pool_site_at && last[-1] >= here)
        last--;
    npool_sites = (int) (last - pool_site_at);
}

/* Whether the value on top is a wide constant, and the bits it is at
 * `type`: what the pool can hold. */
static int wide_const_top(Type type, uint64_t *bits)
{
    Value *v = vsp - 1;

    if (v->kind != VAL_WIDE && !val_number(v->kind))
        return 0;
    *bits = const_as(v, type);

    return 1;
}

/* A four-byte integer operator with a constant on the right, done on the
 * left's bytes in its scratch slot when that is shorter than the call: the
 * call wants the constant in a slot of its own, sixteen bytes, and the two
 * pointers and the call, ten more.
 *
 * & | ^ go a byte at a time through A, and leave the bytes the constant
 * does not change alone -- `c & 0xff` is the low byte kept and three
 * zeros. A shift by one is four rotates of the slot's bytes in place, and a
 * shift by whole bytes is three moved through IY and the rest filled, with
 * zero or the sign. IY reads and writes three bytes, so the move may read
 * up to two bytes past the slot either side, which are the frame's; it
 * writes only the slot's. Returns 0, having emitted nothing, for anything
 * else, or where the slot's bytes are out of a displacement's reach. */
#define LONG_CALL_BYTES 26

static int long_const_bytes(int op, Type result)
{
    Value *r = vsp - 1;
    unsigned char b[8];
    uint64_t c;
    int cost = 0, i, k = 0, low, left, src, zeros = 0;

    if (type_wide_bytes(result) != 4 || type_float(result)
        || (r->kind != VAL_WIDE && !val_number(r->kind)))
        return 0;
    c = const_as(r, result);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (i = 0; i != 8; i++)
        b[i] = (unsigned char) (c >> (i * 8));
#else
    memcpy(b, &c, sizeof b);
#endif

    switch (op) {
    case TK_AMP:
        for (i = 0; i != 4; i++)
            if (b[i] == 0)
                zeros++;
            else if (b[i] != 0xff)
                cost += 8;
        cost += zeros ? 1 + 3 * zeros : 0;
        break;
    case TK_PIPE:
        for (i = 0; i != 4; i++)
            cost += b[i] == 0 ? 0 : b[i] == 0xff ? 4 : 8;
        break;
    case TK_CARET:
        for (i = 0; i != 4; i++)
            cost += b[i] == 0 ? 0 : b[i] == 0xff ? 7 : 8;
        break;
    case TK_SHL:
    case TK_SHR:
        k = b[0];
        if (b[1] || b[2] || b[3] || !(k == 1 || k == 8 || k == 16 || k == 24))
            return 0;
        cost = 16;
        break;
    default:
        return 0;
    }
    if (cost >= LONG_CALL_BYTES)
        return 0;

    /* The spills first, as vbinop_long has them, since they move where
     * the scratch is free from. Done for nothing if the slot is out of
     * reach; the call does them too. */
    save_regs_below(2);
    low = spill_lowest(2, 0);
    left = slot_at(low, 4);
    if (!disp_fits(left - 2) || !disp_fits(left + 5))
        return 0;

    vdrop();                            /* the constant */

    /* A shift by whole bytes of a value already in a slot of its width
     * reads it there, rather than copying it into the scratch to move it
     * again. */
    src = left;
    if ((op == TK_SHL || op == TK_SHR) && k != 1
        && (vsp - 1)->kind == VAL_LOCAL && !(vsp - 1)->bits
        && type_wide(vsp[-1].type) && type_wide_bytes(vsp[-1].type) == 4
        && disp_fits(vsp[-1].val - 2) && disp_fits(vsp[-1].val + 5))
        src = (vsp - 1)->val;
    else
        materialise_long(left, result);
    vdrop();
    spill_used = low + 4;

    switch (op) {
    case TK_AMP:
        for (i = 0; i != 4; i++)
            if (b[i] != 0 && b[i] != 0xff) {
                ld_a_ix(left + i);
                and_a_imm(b[i]);
                ld_ix_a(left + i);
            }
        if (zeros) {
            out_byte(0xaf);                             /* xor a, a */
            for (i = 0; i != 4; i++)
                if (b[i] == 0)
                    ld_ix_a(left + i);
        }
        break;
    case TK_PIPE:
    case TK_CARET:
        for (i = 0; i != 4; i++) {
            if (b[i] == 0)
                continue;
            if (op == TK_PIPE && b[i] == 0xff) {
                out_byte2(0xdd, 0x36);                  /* ld (ix+d), 0xff */
                out_byte2(left + i, 0xff);
                continue;
            }
            ld_a_ix(left + i);
            if (op == TK_CARET && b[i] == 0xff)
                out_byte(0x2f);                         /* cpl */
            else if (op == TK_CARET)
                xor_a_imm(b[i]);
            else
                or_a_imm(b[i]);
            ld_ix_a(left + i);
        }
        break;
    case TK_SHL:
        if (k == 1) {
            out_byte2(0xdd, 0xcb);                      /* sla (ix+d) */
            out_byte2(left, 0x26);
            for (i = 1; i < 4; i++) {
                out_byte2(0xdd, 0xcb);                  /* rl (ix+d) */
                out_byte2(left + i, 0x16);
            }
            break;
        }
        k /= 8;
        out_byte3(0xdd, 0x31, src + 1 - k);             /* ld iy, (ix+d) */
        out_byte3(0xdd, 0x3e, left + 1);                /* ld (ix+d), iy */
        out_byte(0xaf);                                 /* xor a, a */
        for (i = 0; i < k; i++)
            ld_ix_a(left + i);
        break;
    case TK_SHR:
        if (k == 1) {
            out_byte2(0xdd, 0xcb);                      /* srl or sra (ix+d) */
            out_byte2(left + 3, type_unsigned(result) ? 0x3e : 0x2e);
            for (i = 2; i >= 0; i--) {
                out_byte2(0xdd, 0xcb);                  /* rr (ix+d) */
                out_byte2(left + i, 0x1e);
            }
            break;
        }
        k /= 8;
        if (type_unsigned(result)) {
            out_byte(0xaf);                             /* xor a, a */
        } else {
            ld_a_ix(src + 3);
            out_byte2(0x17, 0x9f);                      /* rla; sbc a, a */
        }
        out_byte3(0xdd, 0x31, src + k);                 /* ld iy, (ix+d) */
        out_byte3(0xdd, 0x3e, left);                    /* ld (ix+d), iy */
        for (i = 4 - k; i < 4; i++)
            ld_ix_a(left + i);
        break;
    }
    vpush(VAL_LOCAL, result, left);

    return 1;
}

/* The last wide operators that went through a routine, each one where it
 * began building its left operand, what that was, and how its right one is
 * read. An assignment of the answer to a variable straight after takes them
 * back and does them again into the variable: the first one's left copied
 * there -- or not at all, if it is the variable -- and each routine working
 * on it where it is, where each built its answer in scratch and the last
 * one's was copied. `x = x * k + 1` on a long had two copies of twelve bytes
 * and has none.
 *
 * The ones taken back are a chain: each one's left operand is the answer
 * of the one before, and it begins where that one ended, so that nothing
 * else was written between them. */
#define LOPS 4
typedef struct {
    int      at, nrt, spill_used, which, pooled, right, n, slot, end;
    unsigned epoch;
    uint64_t bits;
    Type     result;
    Value    left;
} LongOp;

static LongOp  lops[LOPS];
static LongOp *lop_top;                 /* past the last, or NULL: none */

/* See lops: 1 if the value on top, going into the wide variable at
 * `offset`, was made again there. Not if a right operand is read from where
 * the variable is: the routine would write it while it read it. */
int long_into(int offset, Type type)
{
    Value *top = vsp - 1;
    LongOp *last, *first, *op;

    if (!lop_top)
        return 0;
    last = lop_top - 1;
    if (last->end != out_here() || last->epoch != out_rewinds
        || top->kind != VAL_LOCAL || top->val != last->slot || top->bits
        || type_wide_bytes(type) != last->n
        || type_float(type) != type_float(last->result))
        return 0;

    /* Back along the chain, as far as it goes. By pointer, as everything
     * here is: an index into these is a multiply on the Agon, and this is
     * asked at every store of a wide value. */
    for (first = last; first > lops; first--) {
        const LongOp *prev = first - 1;

        if (prev->end != first->at || prev->epoch != first->epoch
            || first->left.kind != VAL_LOCAL || first->left.val != prev->slot
            || prev->n != first->n)
            break;
    }
    for (op = first; op <= last; op++)
        if (offset == op->slot
            || (!op->pooled && offset < op->right + op->n
                && op->right < offset + op->n))
            return 0;

    vdrop();
    out_rewind(first->at);
    nrt_fixups = first->nrt;
    spill_used = first->spill_used;
    *vsp++ = first->left;
    vtop++;
    lop_top = NULL;
    materialise_long(offset, first->result);
    vdrop();
    for (op = first; op <= last; op++) {
        lea_rr_ix(R_HL, offset);
        if (op->pooled)
            ld_rr_pool(R_DE, op->bits, op->n);
        else
            lea_rr_ix(R_DE, op->right);
        rt_call(op->which);
    }
    vpush(VAL_LOCAL, type, offset);

    return 1;
}

void vbinop_long(int op, Type result)
{
    int which;
    int left, right;

    /* Anything else live in a register has to come out first. The two lea
     * instructions below load HL and DE with addresses, behind the register
     * allocator's back -- it is not told, because these are not values it
     * will ever be asked for. A result of an earlier operator sitting in DE
     * was overwritten by the second of them, so `(a == 1) + (b == 2)` lost
     * the first comparison. The top two are the operands and are exempt:
     * they are about to be copied into the frame and dropped. */
    int n = type_wide_bytes(result);
    int low, rstart, in_place, pooled;
    uint64_t bits = 0;
    LongOp *lop;

    /* Both constants: worked out here, and nothing emitted. */
    if (vconst_pair()) {
        uint64_t a = const_as(vsp - 2, result), b = const_as(vsp - 1, result);
        int table = wide_table_of(vsp - 2, vsp - 1);
        uint64_t folded;
        int done = type_float(result) ? fold_wide_float(op, a, b, &folded)
                                      : fold_wide_int(op, result, a, b,
                                                      &folded);

        if (done) {
            vdrop();
            vdrop();
            if (table >= 0)
                nwide_consts = table;   /* the operands' entries are dead */
            if (!wide_push(folded, result, 0))
                wide_to_slot(folded, result);

            return;
        }
    }

    /* A constant on the left of an operator that does not care which side
     * is which goes to the right, where the pool or the bytes can have it:
     * on the left it was written into a slot of its own. */
    if ((vsp[-2].kind == VAL_WIDE || val_number(vsp[-2].kind))
        && vsp[-1].kind != VAL_WIDE && !val_number(vsp[-1].kind)
        && !type_float(result)
        && (op == TK_PLUS || op == TK_STAR || op == TK_AMP || op == TK_PIPE
            || op == TK_CARET))
        vswap();

    if (long_const_bytes(op, result))
        return;

    /* A float take away a constant is the float plus the constant with its
     * sign turned over -- exactly, in every case IEEE has, zeros and NaNs
     * included -- and the add only reads its right operand, which lets the
     * constant be read from the pool: the subtract writes its right. */
    if (type_float(result) && op == TK_MINUS
        && (vsp[-1].kind == VAL_WIDE || val_number(vsp[-1].kind))) {
        uint64_t neg = const_as(vsp - 1, result) ^ 0x80000000u;

        vdrop();
        vpush_const_wide((uint32_t) neg, 0, result);
        op = TK_PLUS;
    }

    /* After the spills, not before: a register the call below puts in the
     * frame is a value under the operands, and the floor has to know about
     * it. */
    save_regs_below(2);

    which = long_helper(op, result);
    if (which < 0)
        acc_error_at(tok_line, "the operator %s is not implemented for %s yet",
                     tok_spelling(op), type_float(result) ? "float" : "long");

    /* A right operand already in the frame at the right width is read where
     * it lies: the routine only reads through DE. That is a copy of the
     * value saved, and a scratch slot, on most of the wide operators a
     * program has -- `a + b` now copies a and reads b. */
    in_place = !helper_writes_right(which) && wide_in_place(vsp - 1, result, n);
    low = spill_lowest(2, in_place);

    /* A right operand read in place is live while the routine runs, and
     * nothing is put below a live slot -- but the answer can go where the
     * left operand was even so, as long as the two do not overlap: the
     * routine reads the right while it writes the left. The left is then
     * built where it already is, where it was copied above the right. */
    if (in_place) {
        int size, rs = spill_start_of(vsp - 1, &size), l0 = spill_lowest(2, 0);

        if (rs < 0 || l0 + n <= rs || rs + size <= l0)
            low = l0;
    }
    rstart = spill_used > low + n ? spill_used : low + n;

    /* The right operand is built first, because building the left one may
     * need HL and the right may still be an expression on the stack. The
     * left goes where the answer is to be, which is at or below where the
     * operands are. */
    left = slot_at(low, n);
    pooled = !in_place && !helper_writes_right(which)
             && wide_const_top(result, &bits) && npool < POOL_MAX;
    if (pooled) {
        vdrop();                        /* read from the pool, below */
        right = 0;
        if (low + n > spill_used)
            spill_used = low + n;
    } else if (in_place) {
        right = (vsp - 1)->val;
        vdrop();
        if (low + n > spill_used)
            spill_used = low + n;
    } else {
        right = slot_at(rstart, n);
        spill_used = rstart + n;
        materialise_long(right, result);
        vdrop();
    }

    if (!lop_top)
        lop_top = lops;
    if (lop_top == lops + LOPS) {
        memmove(lops, lops + 1, (LOPS - 1) * sizeof *lops);
        lop_top--;
    }
    lop = lop_top++;
    lop->at = out_here();
    lop->nrt = nrt_fixups;
    lop->spill_used = spill_used;
    lop->left = vsp[-1];
    lop->which = which;
    lop->pooled = pooled;
    lop->bits = bits;
    lop->right = right;
    lop->n = n;
    lop->result = result;

    materialise_long(left, result);
    vdrop();

    lea_rr_ix(R_HL, left);
    if (pooled)
        ld_rr_pool(R_DE, bits, n);
    else
        lea_rr_ix(R_DE, right);
    rt_call(which);
    lop->slot = left;
    lop->end = out_here();
    lop->epoch = out_rewinds;

    spill_used = low + n;               /* the answer, and nothing else */
    vpush(VAL_LOCAL, result, left);
}

/* A float comparison comes back as a code in A -- 0 below, 1 equal, 2 above,
 * 3 unordered -- and this turns it into the 0 or 1 the language wants.
 *
 * Four outcomes rather than three is what NaN costs. A NaN is not below,
 * equal to or above anything, itself included, so `x < y` and `x >= y` are
 * both false when either operand is one. No ordering of three can say that,
 * which is why the routine hands back a code instead of leaving the flags for
 * a branch to read the way the integer comparisons do.
 *
 * Each test below leaves the answer in a flag that cmp_equal or cmp_unsigned
 * can already turn into a value. `>=` is the only one that needs two
 * instructions: it is true for the codes 1 and 2, which is the pair a `dec`
 * brings to 0 and 1 and an unsigned compare against 2 then catches. */
static void cmp_from_code(int op)
{
    switch (op) {
    case TK_LT:
        out_byte2(0xfe, 0);            /* cp a, 0 */
        cmp_equal(1);

        return;
    case TK_EQ:
        out_byte2(0xfe, 1);
        cmp_equal(1);

        return;
    case TK_GT:
        out_byte2(0xfe, 2);
        cmp_equal(1);

        return;
    case TK_NE:
        out_byte2(0xfe, 1);
        cmp_equal(0);

        return;
    case TK_LE:
        out_byte2(0xfe, 2);            /* below or equal: 0 or 1 */
        cmp_unsigned(1);

        return;
    case TK_GE:
        out_byte(0x3d);                         /* dec a */
        out_byte2(0xfe, 2);
        cmp_unsigned(1);

        return;
    }

    acc_error("internal: %s is not a comparison", tok_spelling(op));
}

/* Comparing two four-byte values, whether they are longs or floats.
 *
 * A float goes through fkey first, which rewrites it as the unsigned integer
 * that sorts the way it does. After that it is the same comparison as any
 * other four bytes, which is the whole reason for doing it that way: the
 * ordering of floats is not a second four-byte compare that knows about
 * exponents, it is this one with the operands prepared. */
void vcmp_wide(int op, Type operand)
{
    int floating = type_float(operand);
    int n = type_wide_bytes(operand);
    int low, rstart, in_place, pooled;
    int left, right;
    uint64_t bits = 0;

    /* Anything else live in a register has to come out first. The two lea
     * instructions below load HL and DE with addresses, behind the register
     * allocator's back -- it is not told, because these are not values it
     * will ever be asked for. A result of an earlier operator sitting in DE
     * was overwritten by the second of them, so `(a == 1) + (b == 2)` lost
     * the first comparison. The top two are the operands and are exempt:
     * they are about to be copied into the frame and dropped. */
    /* Both constants: the answer is a 0 or a 1 the compiler knows. */
    if (vconst_pair()) {
        uint64_t a = const_as(vsp - 2, operand), b = const_as(vsp - 1, operand);
        int answer;

        if (floating) {
            /* -1, 0, 1 or 2 for a pair with a NaN in it, which answers no
             * to every comparison but `!=`. */
            int cmp = float_compare((uint32_t) a, (uint32_t) b);

            if (cmp == 2)
                answer = op == TK_NE;
            else
                answer = op == TK_EQ ? cmp == 0 : op == TK_NE ? cmp != 0
                       : op == TK_LT ? cmp <  0 : op == TK_GT ? cmp >  0
                       : op == TK_LE ? cmp <= 0 : cmp >= 0;
        } else if (type_unsigned(operand)) {
            answer = op == TK_EQ ? a == b : op == TK_NE ? a != b
                   : op == TK_LT ? a <  b : op == TK_GT ? a >  b
                   : op == TK_LE ? a <= b : a >= b;
        } else {
            int64_t x = wide_signed(a, n), y = wide_signed(b, n);

            answer = op == TK_EQ ? x == y : op == TK_NE ? x != y
                   : op == TK_LT ? x <  y : op == TK_GT ? x >  y
                   : op == TK_LE ? x <= y : x >= y;
        }
        vdrop();
        vdrop();
        vpush_const(answer, TY_INT);

        return;
    }

    save_regs_below(2);

    /* Equal to zero, or not: the bytes ORed together in A, which leaves Z
     * for cmp_value -- and for a branch after it -- rather than a zero put
     * in the pool and the compare routine called. A variable is read where
     * it is. */
    if (!floating && (op == TK_EQ || op == TK_NE)
        && wide_const_top(operand, &bits)
        && (n == 8 ? bits == 0 : (uint32_t) bits == 0)) {
        Value *l = vsp - 2;
        int src, own = l->kind == VAL_LOCAL && !l->bits && type_wide(l->type)
                       && type_wide_bytes(l->type) == n, i;

        low = spill_lowest(2, 0);
        src = own ? l->val : slot_at(low, n);
        if (disp_fits(src) && disp_fits(src + n - 1)) {
            vdrop();                            /* the zero */
            if (!own)
                materialise_long(src, operand);
            vdrop();
            if (!own)
                spill_used = low;
            ld_a_ix(src);
            for (i = 1; i < n; i++)
                out_byte3(0xdd, 0xb6, src + i);     /* or a, (ix+d) */
            cmp_value(op, 1);

            return;
        }
    }

    /* As in vbinop_long: the integer comparisons only read through DE, so a
     * right operand already in the frame is compared where it lies. The
     * float one rewrites both operands and cannot. */
    in_place = !floating && wide_in_place(vsp - 1, operand, n);
    low = spill_lowest(2, in_place);
    rstart = spill_used > low + n ? spill_used : low + n;

    left = slot_at(low, n);
    pooled = !floating && !in_place && wide_const_top(operand, &bits)
             && npool < POOL_MAX;
    if (pooled) {
        vdrop();                        /* read from the pool, below */
        right = 0;
        if (low + n > spill_used)
            spill_used = low + n;
    } else if (in_place) {
        right = (vsp - 1)->val;
        vdrop();
        if (low + n > spill_used)
            spill_used = low + n;
    } else {
        right = slot_at(rstart, n);
        spill_used = rstart + n;
        materialise_long(right, operand);
        vdrop();
    }

    materialise_long(left, operand);
    vdrop();


    /* A float comparison answers in four ways and not three, so it does not
     * go through the same tail as the integer ones. */
    if (floating) {
        lea_rr_ix(R_HL, left);
        lea_rr_ix(R_DE, right);
        rt_call(RT_FCMP);
        cmp_from_code(op);
        spill_used = low;               /* the answer is in HL */
        vpush_reg(R_HL);

        return;
    }

    /* `a > b` is `b < a` and `a <= b` is `b >= a`, done by which address goes
     * in which register rather than by a second routine. */
    if (tok_pair(op, TK_GT)) {
        int swap = left;

        left = right;
        right = swap;
        op = (op == TK_GT) ? TK_LT : TK_GE;
        if (pooled) {
            ld_rr_pool(R_HL, bits, n);  /* the constant is on the left now */
            lea_rr_ix(R_DE, right);
        }
    } else if (pooled) {
        lea_rr_ix(R_HL, left);
        ld_rr_pool(R_DE, bits, n);
    }
    if (!pooled) {
        lea_rr_ix(R_HL, left);
        lea_rr_ix(R_DE, right);
    }

    if (tok_pair(op, TK_EQ)) {
        rt_call(type_eight(operand) ? RT_LLCMPEQ : RT_LCMPEQ);
        cmp_equal(op == TK_EQ);
    } else {
        /* The last subtract of the four leaves S, P/V and C describing the
         * whole width, so the same branch sequence the 24-bit comparisons use
         * reads them unchanged. */
        rt_call(type_eight(operand) ? RT_LLCMPORD : RT_LCMPORD);

        /* A key is unsigned by construction: that is what makes the negative
         * floats sort below the positive ones. */
        if (floating || type_unsigned(operand))
            cmp_unsigned(op == TK_LT);
        else
            cmp_signed(op == TK_LT);
    }
    spill_used = low;
    vpush_reg(R_HL);
}
