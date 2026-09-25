/*
 * eZ80 code generation.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Values are not turned into instructions when they are parsed, only when
 * something needs them in a register. A constant stays a number, a local
 * stays a frame offset, and `x + 1` loads x once and adds an immediate rather
 * than loading both and adding registers.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "rt_helpers.h"

/* ------------------------------------------------------------------ */
/* instructions                                                        */

/* Z80 numbers a register pair in a two-bit field: BC 0, DE 1, HL 2. acc
 * numbers them HL, DE, BC so that HL -- the one everything returns in -- is
 * register zero. This maps between the two. */
/* Kept already shifted into the bits of an opcode where the register goes.
 * Every use is `reg_code * 0x10` added to a base opcode, and a byte shift by
 * four is a call into the runtime on this target -- once for every register
 * load, store, push and pop the compiler emitted. */
static const unsigned char reg_code[NREGS] = { 0x20, 0x10, 0x00 };

/* That a frame offset fits the signed byte of (ix+d). Tested inline, because
 * it runs for every local touched and the call to test it opened a frame; as
 * one unsigned compare rather than two signed ones, because a signed compare
 * on this target is a call to repair the flags. */
#define disp_fits(d)  ((unsigned) ((d) + 128) <= 255u)


static void ld_rr_imm(int reg, int imm)    /* ld rr, nn */
{
    out_opcode24(0x01 + reg_code[reg], imm);
}

static int far_base(int disp);

static inline __attribute__((always_inline))
void ld_rr_ix(int reg, int disp)           /* ld rr, (ix+d) */
{
    if (disp_fits(disp))
        out_byte3(0xdd, 0x07 + reg_code[reg], disp);
    else
        out_byte3(0xfd, 0x07 + reg_code[reg], far_base(disp));
}

static void ld_ix_rr(int disp, int reg)    /* ld (ix+d), rr */
{
    if (disp_fits(disp))
        out_byte3(0xdd, 0x0f + reg_code[reg], disp);
    else
        out_byte3(0xfd, 0x0f + reg_code[reg], far_base(disp));
}

static void push_rr(int reg) { out_byte(0xc5 + reg_code[reg]); }
static void pop_rr(int reg)  { out_byte(0xc1 + reg_code[reg]); }

static void add_hl_rr(int reg) { out_byte(0x09 + reg_code[reg]); }
static void sbc_hl_rr(int reg) { out_byte2(0xed, 0x42 + reg_code[reg]); }
static void or_a_a(void)     { out_byte(0xb7); }

/* There is no ld rr, rr' on this chip, so a move is a push and a pop -- two
 * bytes and nine cycles to copy a register, which zap spent six percent of
 * its time doing.
 *
 * Between HL and DE there is `ex de, hl`, one byte and one cycle. It swaps
 * rather than copies, which is why this used to avoid it: the allocator is
 * entitled to believe a register still holds what it held. So the swap is
 * told to the allocator rather than hidden from it -- whatever either
 * register holds changes places with the other -- and then nothing is
 * believed that is not true. A value that was in the destination used to be
 * quietly destroyed by the pop; now it moves to the source, which is a
 * better answer as well as a cheaper one.
 *
 * Callers set the moved value's register themselves afterwards, which is
 * what the swap has already done: saying it twice costs nothing. */
static void ex_de_hl(void);
static void check_reg_free(int reg);

static void mov_rr(int dst, int src)
{
    if (dst == src)
        return;

    if ((dst == R_HL && src == R_DE) || (dst == R_DE && src == R_HL)) {
        check_reg_free(dst);
        ex_de_hl();

        return;
    }

    push_rr(src);
    pop_rr(dst);
}

/* IY pointed at a frame slot that (ix+d) cannot reach, and the displacement
 * left over for the access itself.
 *
 * (ix+d) carries one signed byte, so the frame reaches 128 bytes either side
 * of the frame pointer. Nearly everything is inside that: a function's
 * locals stop at 96 bytes and the scratch a statement wants is four on
 * average. What is outside it is a function with both many locals and a
 * statement that spills hard -- a call to a variadic function with forty
 * arguments, say -- and that used to be refused outright, which is a ceiling
 * on how large a function may be rather than anything C says.
 *
 * IY is the backend's own scratch: nothing the register allocator holds ever
 * lives there, and the two sequences that do use it -- a dereference and a
 * narrowing conversion -- touch no frame slot in between. So pointing it at
 * the slot needs no register saved and moves no value the allocator is
 * holding. `lea iy, ix+d` reaches 128 and they chain, so two reach 256 and
 * three 384, which is as deep as a frame can go before the arrays below it
 * would have run the stack out anyway.
 *
 * The displacement that comes back is what is left after the hops, so a run
 * of bytes in one slot -- which is what every eight-byte value is -- could
 * pay for the pointer once. It does not yet: each access lays its own down,
 * which is the simple thing and is only paid by the functions that were
 * refused before. */
static int far_base(int disp)
{
    int step = disp < 0 ? -128 : 127;

    out_byte3(0xed, 0x55, step);                /* lea iy, ix+step */
    disp -= step;
    while (!disp_fits(disp)) {
        out_byte3(0xed, 0x33, step);            /* lea iy, iy+step */
        disp -= step;
    }

    return disp;
}

static int  spill_slot(void);
static void vpush_scratch(Type type, int slot);
static int  force_reg(Value *val);
static int  reg_owner(int reg, const Value *except);

/* The result of a void function, used as though it were a value. Found where
 * a value is loaded or converted -- the paths any use of it ends up on --
 * and on the branches of them that read from the frame, which the common
 * cases have already left, so that it costs them nothing. */
__attribute__((noinline, noreturn))
static void void_used(void)
{
    acc_error_at(tok_line, "a void function returns nothing, so its result "
                           "cannot be used");
}
/* A struct used where a number was wanted: an operand, a condition, a
 * conversion. A struct value is its address, in a register or a frame slot,
 * and everything that uses one as a struct -- a member, a copy, an argument
 * -- relabels it as a pointer before it loads it. So anything that loads one
 * still labelled a struct is using it as a number, and that is found where
 * values are loaded and converted, as a void result is. */
__attribute__((noinline, noreturn))
static void struct_used(void)
{
    acc_error_at(tok_line, "a struct or union cannot be used as a number");
}

static int  long_scratch(Type type);
static void wide_to_slot(uint64_t bits, Type type);
static void wide_bytes_at(int disp, uint64_t bits, int n);
static uint64_t const_as(const Value *v, Type to);
static int  wide_push(uint64_t bits, Type type, int ext);
static int  trunc_int(int value);
static void bool_from(void);
static void bitfield_read(void);
static void bitfield_write(void);
static void call_through(void);
static void check_no_float_mix(Type to, const Value *from);
static void no_float_address(const Value *from);
static void materialise_long(int disp, Type type);
static void evict_reg(int reg);
static void vunary_long(int which, Type type);
static Type common_wide(Type left, Type right);
static int  is_comparison(int op);
static void vcmp_pointer_check(Type left, Type right);
static void convert_int_to_float(void);
static void convert_float_to_int(Type to);
static void force_into(Value *target, int want);
static int  needs_helper(int op);
static void rt_call(int which);

/* ------------------------------------------------------------------ */
/* narrow widths                                                       */

/* `sbc hl, hl` is the whole trick. It leaves HL as 0 or -1 depending on the
 * carry, and it is the only way to set all three bytes at once: the upper
 * byte of HL has no name, so `ld h, a` reaches two thirds of the register and
 * there is no `ld hlu, a` to reach the rest.
 *
 * So widening a byte is: get the sign into the carry, `sbc hl, hl` to fill
 * the register with it, then drop the byte back into L. It is what agondev
 * emits, arrived at the same way -- there is not another one.
 */

/* The byte forms, each of which is the same instruction through IY when the
 * slot is out of (ix+d)'s reach. */
static void frame_byte(int op, int disp)
{
    if (disp_fits(disp))
        out_byte3(0xdd, op, disp);
    else
        out_byte3(0xfd, op, far_base(disp));
}

static void ld_a_ix(int disp)   { frame_byte(0x7e, disp); }
static void ld_e_ix(int disp)   { frame_byte(0x5e, disp); }
static void ld_l_ix(int disp)   { frame_byte(0x6e, disp); }
static void ld_h_ix(int disp)   { frame_byte(0x66, disp); }
static void ld_ix_a(int disp)   { frame_byte(0x77, disp); }
static void ld_ix_l(int disp)   { frame_byte(0x75, disp); }
static void ld_ix_h(int disp)   { frame_byte(0x74, disp); }
static void ld_l_a(void)        { out_byte(0x6f); }
static void ld_h_a(void)        { out_byte(0x67); }
static void ld_a_l(void)        { out_byte(0x7d); }
static void ld_a_h(void)        { out_byte(0x7c); }
static void rlc_l(void)         { out_byte2(0xcb, 0x05); }
static void ld_a_hl(void)       { out_byte(0x7e); }      /* ld a, (hl) */
static void ld_hl_a(void)       { out_byte(0x77); }      /* ld (hl), a */
static void inc_hl(void)        { out_byte(0x23); }
static void dec_hl(void)        { out_byte(0x2b); }
static void inc_de(void)        { out_byte(0x13); }
static void ld_de_a(void)       { out_byte(0x12); }      /* ld (de), a */
static void ex_de_hl(void)      { out_byte(0xeb); }
static void ld_hl_ind_hl(void)  { out_byte2(0xed, 0x27); }  /* ld hl, (hl) */
static void ld_ind_hl_de(void)  { out_byte2(0xed, 0x1f); }  /* ld (hl), de */
static void sbc_hl_hl(void)     { out_byte2(0xed, 0x62); }

/* HL = the sign of A, in all three bytes. Clobbers L on the way. */
static void fill_hl_with_sign_of_a(void)
{
    ld_l_a();
    rlc_l();                    /* bit 7 into the carry */
    sbc_hl_hl();                /* 0 or -1, upper byte included */
}

static void fill_hl_with_zero(void)
{
    or_a_a();                   /* clear the carry */
    sbc_hl_hl();
}

/* HL = HL OP a constant, written out rather than called for.
 *
 * The helpers take both operands on the stack and walk them a byte at a
 * time, because on this chip only the low two bytes of HL have names and the
 * third has no way to be reached. A constant does not need that: every byte
 * of it is known here, and a byte that is the operator's identity -- 0xff for
 * AND, 0 for OR and XOR -- is one there is no code to write at all.
 *
 * Which is nearly always the top byte. In zap, 83% of the ANDs executed have
 * a constant on the right and every mask among the hot ones is a bit or two
 * in the low byte: `& 1`, `& 0x10`, `& 0x0f`. So the third byte is left as
 * it is, or the whole of HL is cleared, and neither needs a name for it.
 *
 * Returns 0 for the rest -- a constant that really does reach into the third
 * byte -- and the caller falls back to the helper.
 *
 * AND clears the carry, so a mask confined to the low byte can zero the two
 * bytes above it with the sbc hl, hl that (unsigned char) already uses, and
 * comes to six bytes against the eight of loading BC and calling. */
static int and_a_imm(int v)  { out_byte2(0xe6, v & 0xff); return 0; }
static int or_a_imm(int v)   { out_byte2(0xf6, v & 0xff); return 0; }
static int xor_a_imm(int v)  { out_byte2(0xee, v & 0xff); return 0; }
static void flags_say_nonzero(int from);
static void jumps_forget(int from);
static void cmp_value(int op, int is_unsigned);

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
        ld_a_l();
        and_a_imm(c0);
        at = out_here();
        sbc_hl_hl();            /* and cleared the carry, so this is 0 */
        ld_l_a();
        flags_say_nonzero(at);

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

/* HL = HL * a constant, as doublings and additions rather than a call.
 *
 * Every multiply zap executes has a constant on the right, and the one it
 * does most often is by 13 -- the width of a struct it keeps an array of,
 * which is what a multiply in C source usually is once the subscripts are
 * counted. 13 is 1101 in binary, so: start with x, and for each bit below
 * the top one double what is in hand and add x back where the bit is set.
 * Nine bytes against the eight of loading BC and calling, and about a
 * thirtieth of the time.
 *
 * DE holds the untouched x and is saved and restored around the whole thing,
 * so this needs no register to be free and cannot disturb what the allocator
 * is holding: the push of DE is consumed by the pop at the end, and the push
 * of HL in between by the pop that loads DE.
 *
 * A power of two needs no copy of x at all, just the doublings, and comes to
 * fewer bytes than the call as well as less time.
 *
 * Returns 0 for a constant that would take more code than it saves, and for
 * a negative one, which would want a negation on the end. */
static void add_hl_hl(void) { out_byte(0x29); }

#define STEP_MAX 4             /* inc/dec, before ld bc,n and add wins */
#define MUL_MAX_STEPS 12        /* doublings plus additions, before it is
                                 * cheaper to let the helper do it */

/* The powers of two a constant multiplier can hold, lowest first. */
static const unsigned powers_of_two[16] = {
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
    0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000
};

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

/* Load a local of the given type into HL, widened to int. */
static void load_narrow(int disp, Type type)
{
    if (type_unsigned(type)) {
        fill_hl_with_zero();
        ld_l_ix(disp);
        if (type_size(type) == 2)
            ld_h_ix(disp + 1);

        return;
    }

    if (type_size(type) == 1) {
        ld_a_ix(disp);
        fill_hl_with_sign_of_a();
        ld_l_a();

        return;
    }

    /* The high byte first, since it decides the sign, and the low one
     * straight into L once the fill has been done -- nothing kept aside in
     * another register, which the allocator may be holding a value in. E
     * was, once, and a signed short read while a value sat in DE cost that
     * value its low byte. */
    ld_a_ix(disp + 1);
    fill_hl_with_sign_of_a();
    ld_h_a();
    ld_l_ix(disp);
}

/* The narrow loads work in HL, because that is the only register `sbc hl, hl`
 * can fill. Getting the result somewhere else goes through the stack rather
 * than through the allocator: evicting HL would move whatever the caller had
 * put there, and vbinop has just put its left operand in it. Four bytes to
 * leave the allocator's arrangement exactly as it was. */
static void load_narrow_into(int reg, int disp, Type type)
{
    if (reg == R_HL) {
        load_narrow(disp, type);

        return;
    }
    push_rr(R_HL);
    load_narrow(disp, type);
    push_rr(R_HL);
    pop_rr(reg);
    pop_rr(R_HL);
}

/* Store the low bytes of HL into a local of the given type. */
static void store_narrow(int disp, Type type)
{
    if (type_size(type) == 1) {
        ld_a_l();
        ld_ix_a(disp);

        return;
    }
    ld_ix_l(disp);
    ld_ix_h(disp + 1);
}

/* Narrow what is in HL to `to`, then widen it back, which is what a C
 * conversion to a narrow type leaves behind. */
static void convert_in_hl(Type to)
{
    if (type_size(to) >= ACC_INT_SIZE)
        return;

    if (type_size(to) == 1) {
        ld_a_l();
        if (type_unsigned(to))
            fill_hl_with_zero();
        else
            fill_hl_with_sign_of_a();
        ld_l_a();

        return;
    }

    /* The low byte is kept in IY, the backend's own scratch, rather than
     * in E: DE may be holding a value, and this would have cost it its low
     * byte. */
    out_byte3(0xe5, 0xfd, 0xe1);        /* push hl; pop iy */
    ld_a_h();
    if (type_unsigned(to))
        fill_hl_with_zero();
    else
        fill_hl_with_sign_of_a();
    ld_h_a();
    out_byte3(0xfd, 0x7d, 0x6f);        /* ld a, iyl; ld l, a */
}

/* The type this function was declared to return, so that `char f()` giving
 * back 300 gives back 44 as C says it must. */
static Type return_type = TY_INT;
static int  return_ext;         /* the struct it returns, when it does */

/* agondev hands a one-byte result back in A and everything else in HL, and
 * acc matches it -- not as a courtesy but because acc exists so that code can
 * be compiled on the machine, and a library built with agondev has to keep
 * working. test/abi.sh pins the convention; there is no document that states
 * it. A is genuinely a different register rather than a narrower read of HL,
 * which makes this the one place the two sides could quietly disagree. */
#define RETURNS_IN_A(ty) (type_size(ty) == 1)

/* ------------------------------------------------------------------ */
/* the value stack                                                     */

/* How many values an expression may have pending at once. A call's
 * arguments are all pending until the call is made, and C99 5.2.4.1 asks
 * for 127 of them -- 64 here refused YARPGen's drivers, which pass a test
 * function 70 to 90 -- with room above them for what the last one nests. */
#define VSTACK_MAX 256

static Value vstack[VSTACK_MAX];
/* Never negative, and compared as if unsigned -- `vtop == 0`, `(unsigned)
 * vtop < 2` -- because a signed compare is a helper call on this target, and
 * these are in front of every value pushed and popped. test/helpers.sh holds
 * the hot ones to it. */
static int   vtop;               /* number of live entries */

/* One past the top, kept in step with vtop.
 *
 * `vstack[vtop - 1]` is `vstack + (vtop - 1) * 4`, and scaling an index is a
 * helper call on this target -- `call __ishl` for a 4-byte element. The top
 * two entries are reached on every value pushed, every binary operation and
 * every negation, and reaching them through a pointer the stack already has
 * makes the offset a constant instead.
 *
 * It buys nothing where the offset is a variable, as in force_into, because
 * that scale is still a scale. Those are left as subscripts, which say what
 * they mean. */
static Value *vsp = vstack;

/* That nothing the allocator is holding lives in this register.
 *
 * mov_rr leans on it: `ex de, hl` gives the destination what the source
 * held, which is the move that was asked for, but it hands the source
 * whatever the destination held, which is only harmless when that was
 * nothing. Every caller frees the destination first -- force_into evicts it,
 * move_out picks a register that is already free, and the branches are
 * emitted with an empty stack -- so this says so rather than paying at every
 * move to make it true a second time.
 *
 * Checked in the sanitized build the tests run and not in the compiler,
 * which is where vcheck draws the same line. */
static void check_reg_free(int reg)
{
#ifdef ACC_CHECK_VSTACK
    const Value *v;

    for (v = vstack; v < vsp; v++)
        if (v->kind == VAL_REG && v->val == reg)
            acc_error("internal: a register move into %d, which is live", reg);
#else
    (void) reg;
#endif
}

/* Locals are at negative offsets from IX and grow downwards. Arguments are
 * above the saved IX and the return address, so the first one is at ix+6. */
/* The frame is in two parts. The declared locals sit at the top of it and
 * last as long as the function does. Below them is a scratch area the
 * allocator spills registers into, which is reused: it is empty at every
 * statement boundary, because that is where the value stack is empty, so a
 * statement's spills can occupy the same bytes as the last one's.
 *
 * Without the reuse, a function with enough calls in it ran out of frame --
 * every spill took new bytes and never gave them back, so a hundred or so
 * calls put a slot past the -128 that (ix+d) reaches. The compiler refused
 * rather than emitting something wrong, which made it a ceiling on how large
 * a function could be rather than a bug, but a real one.
 *
 * The prologue reserves the locals plus the deepest the scratch area ever
 * got, which is not known until the function ends -- so it is patched, like
 * the frame size always was. */
static int locals_size;          /* the declared locals */
static int spill_used;           /* the scratch in use right now */

/* Slots handed out that the value stack does not point at yet: see
 * spill_free_from. */
#define SPILL_PENDING 4

static struct { int disp, end; } spill_pending[SPILL_PENDING];
static unsigned char nspill_pending;     /* a byte: vpush tests it */
static int spill_peak;           /* the most it ever held */
static int spill_locked;         /* held by something not on the value stack */
static int frame_patch;          /* where the prologue's frame size is written */

/* Local arrays, which go below everything else in the frame.
 *
 * (ix+d) reaches 128 bytes, and the locals and the scratch area have to fit
 * in them. An array is too big to share that -- one of a few hundred bytes
 * declared first would put every scalar after it out of reach -- and it does
 * not need to: an array is only ever used through its address, which is
 * worked out, so it can be anywhere. So the arrays take the frame from where
 * the rest ends, and since where that is depends on the deepest the scratch
 * area gets, which is not known until the function ends, the offset in every
 * address computed is left as a hole and filled in then.
 *
 * An array is known by a number, and the area records where each one ends.
 * Its size can be given after it is first used: `int a[] = { ... }` has its
 * elements stored before the closing brace says how many there are. */
static int arrays_size;          /* bytes of arrays sized so far */
static int *array_end;           /* by array number: its end in the area */
static int narrays, array_end_cap;

typedef struct {
    int at;                      /* the hole: the operand of an ld de, nn */
    int array;                   /* whose address it is */
} ArrayPatch;

static ArrayPatch *array_patches;
static int narray_patches, array_patches_cap;

static int frame_size(void)
{
    return locals_size + spill_peak + arrays_size;
}

static void vcheck(void)
{
    /* The two have to agree, and nothing but a bug can make them disagree.
     * Checked in the sanitized build the tests run, not in the compiler:
     * this is on the path of every value the parser produces. */
#ifdef ACC_CHECK_VSTACK
    if (vsp != vstack + vtop)
        acc_error("internal: the value stack pointer and its count disagree");
#endif
    if ((unsigned) vtop >= VSTACK_MAX)
        acc_error_at(tok_line, "expression is nested too deeply");
}

/* The one place that writes an entry and moves the top, so that the pointer
 * and the count cannot get out of step anywhere else. */
/* Always inlined: every value the compiler handles is pushed through here,
 * and as a call it opened a frame on this target to store three fields. */
static inline __attribute__((always_inline))
void vpush(int kind, Type type, int val)
{
    vcheck();
    vsp->kind = (unsigned char) kind;
    vsp->type = type;
    vsp->val = val;
    vsp->ext = 0;
    vsp->quals = 0;
    vsp->bits = 0;
    vsp++;
    vtop++;
}

void vpush_const(int val, Type type)
{
    vpush(VAL_CONST, type, val);
}

void vset_addr(void)
{
    (vsp - 1)->kind = VAL_ADDR;
}

/* A variable in the bss, as its offset into it. Where the bss starts is
 * added to whatever this folds into, wherever that is written out. */
void vpush_bss(int at, Type type)
{
    vpush_const(at, type);
    (vsp - 1)->kind = VAL_BSS;
}

void vpush_local(int offset, Type type)
{
    vpush(VAL_LOCAL, type, offset);
}

void vpush_reg(int reg)
{
    vpush(VAL_REG, TY_INT, reg);
}

/* Convert the top of the stack to `to`: narrow it and widen it back, which
 * is what C leaves behind after an assignment to a narrow object or a cast. */
void vconvert(Type to)
{
    Value *top = vsp - 1;

    if (vtop == 0)
        acc_error("internal: nothing to convert");
    if (top->kind == VAL_VOID)
        void_used();

    /* Converting to the type it already has is nothing at all, and that is
     * the common case now that every argument of every call comes through
     * here on its way to a parameter. */
    if (top->type == to)
        return;

    /* To _Bool is a comparison with zero, whatever it is from. An address
     * is never zero, and that answer is the same wherever it is loaded. */
    if (to == TY_BOOL) {
        bool_from();

        return;
    }

    /* A constant going to something four bytes or wider is converted here,
     * bits and all: an int becoming a float is the rounding float_from_int
     * does, a long becoming a long long is its sign, and none of it is
     * emitted. Pointers and structs are not arithmetic and keep to the
     * paths below. */
    if ((val_const(top->kind) || top->kind == VAL_WIDE)
        && type_wide(to) && !type_pointer(to) && !type_pointer(top->type)
        && !type_is_struct(to) && !type_is_struct(top->type)) {
        uint64_t bits;

        if (type_float(to))
            no_float_address(top);
        bits = const_as(top, to);

        vdrop();
        if (!wide_push(bits, to, 0))
            wide_to_slot(bits, to);

        return;
    }

    /* Between a float and an integer is a conversion of the value, not of
     * the label on it. Both directions go through an int, so a narrow type
     * widens first and a long is still refused. */
    if (type_float(to) != type_float(top->type)
        && !(val_number(top->kind) && top->val == 0)) {
        if (type_float(to)) {
            no_float_address(top);
            convert_int_to_float();
            if (to != TY_FLOAT)
                (vsp - 1)->type = to;

            return;
        }

        convert_float_to_int(to);

        return;
    }

    check_no_float_mix(to, top);

    if (type_wide(to)) {
        int slot;

        /* Already in the frame and at least as wide: the low bytes are
         * where they were, so it is a relabelling. */
        if (top->kind == VAL_LOCAL && type_wide(top->type)
            && type_wide_bytes(top->type) >= type_wide_bytes(to)) {
            top->type = to;

            return;
        }
        slot = long_scratch(to);
        materialise_long(slot, to);
        vdrop();
        vpush_scratch(to, slot);

        return;
    }
    /* A wide constant on its way down to an int or narrower is that many of
     * its low bytes, worked out here. */
    if (top->kind == VAL_WIDE && !type_float(to)) {
        uint64_t bits = const_as(top, to);

        vdrop();
        vpush_const(trunc_int((int) (bits & 0xffffff)), type_promote(to));
        if (type_size(to) < ACC_INT_SIZE)
            vconvert(to);

        return;
    }

    if (type_size(to) >= ACC_INT_SIZE) {
        /* A struct is as wide as an int, so it is here that one converted
         * to or from anything is caught -- after the common case, a value
         * already of the type it is going to, has left. */
        if (type_is_struct(top->type) || type_is_struct(to))
            struct_used();

        /* Widening. A value still sitting in the frame at its own narrow
         * width has to be loaded before it can be called an int, because the
         * load is what widens it -- relabelling it would have the next load
         * read three bytes of a one-byte object. A constant and a register
         * are already at int width and only need the label. */
        if (type_size(top->type) < ACC_INT_SIZE
            && (top->kind == VAL_LOCAL || top->kind == VAL_ACC))
            force_reg(vsp - 1);
        top->type = to;

        return;
    }

    if (val_const(top->kind)) {
        int bits = type_size(to) * 8;
        int mask = (1 << bits) - 1;

        /* An address cut down to a byte or two is no longer an address: the
         * bytes that are left say nothing about where the thing is, and they
         * would be different had it been put somewhere else. C leaves what
         * comes out to the implementation, and the honest answer on a machine
         * whose addresses are three bytes is that it does not fit. */
        if (val_pending(top->kind))
            acc_error_at(tok_line, "an address is %d bytes and this keeps "
                                   "only %d of them", ACC_INT_SIZE,
                         type_size(to));

        top->val &= mask;
        if (!type_unsigned(to) && (top->val & (1 << (bits - 1))))
            top->val -= mask + 1;
        top->type = type_promote(to);

        return;
    }

    force_into(vsp - 1, R_HL);
    convert_in_hl(to);
    top->type = type_promote(to);
}

Type vtype(void)
{
    if (vtop == 0)
        acc_error("internal: asked the type of nothing");

    return (vsp - 1)->type;
}

int vext(void)
{
    return (vsp - 1)->ext;
}

Type vtype_at(int depth)
{
    if ((unsigned) vtop <= (unsigned) depth)
        acc_error("internal: asked the type of nothing");

    return (vsp - 1 - depth)->type;
}

/* The top, a pointer, as a pointer of another type to the same address: what
 * `&a` makes of an array, which is already its first element's address. */
void vset_type(Type type, int ext)
{
    (vsp - 1)->type = type;
    (vsp - 1)->ext = (unsigned char) ext;
}

/* The extension of the top's type, which a push leaves at none: set by the
 * parser after pushing something whose type came from a declaration that
 * had one. */
void vset_ext(int ext)
{
    (vsp - 1)->ext = (unsigned char) ext;
}

void vset_quals(int quals)
{
    (vsp - 1)->quals = (unsigned char) quals;
}

void vset_bits(int bits)
{
    (vsp - 1)->bits = (unsigned char) bits;
}

int vbits(void)
{
    return (vsp - 1)->bits;
}

int vquals(void)
{
    return (vsp - 1)->quals;
}

/* Whether the top of the stack is a constant, and if so its value and type.
 * What a global's initial value has to come to. */
/* Constants too wide for a Value's val: a long's four bytes, a long long's
 * eight, a float's bits.
 *
 * They are not emitted where they are written. `1L << 20` in a global's
 * initial value has to be worked out here -- a global takes a constant and
 * not code -- and inside a function it is the difference between a call
 * into the runtime and four `ld (ix+d), n`. So a wide constant is a value
 * of its own, VAL_WIDE, whose val indexes this table, and the operators
 * fold two of them into a third.
 *
 * The table is emptied at each statement, as the scratch area is, and holds
 * as many as an expression can have live at once. Running out is not an
 * error: the value is put in the frame instead, which is where it was going
 * before any of this. */
static int64_t wide_signed(uint64_t bits, int width);

#define WIDE_CONSTS 24
static uint64_t wide_consts[WIDE_CONSTS];
static int      nwide_consts;

static uint64_t wide_value(const Value *v)
{
    return wide_consts[v->val];
}

/* A wide value where the frame is what is wanted: a constant is written
 * into a slot of its own, and anything else is already in one. The paths
 * that push arguments and return a result read the slot itself, so this is
 * what they call before they do. */
static void wide_needs_slot(void)
{
    Value *top = vsp - 1;
    uint64_t bits;
    Type type;

    if (top->kind != VAL_WIDE)
        return;
    bits = wide_value(top);
    type = top->type;
    vdrop();
    wide_to_slot(bits, type);
}

/* Where the table can be wound back to once two operands are folded into
 * one: they are the last entries in it, since a value under them on the
 * stack was pushed before them. -1 when neither is in the table. */
static int wide_table_of(const Value *a, const Value *b)
{
    if (a->kind == VAL_WIDE)
        return a->val;
    if (b->kind == VAL_WIDE)
        return b->val;

    return -1;
}

/* Whether the top two values are both constants, which is what lets an
 * operator work its answer out rather than emit it. */
static int vconst_pair(void)
{
    const Value *a = vsp - 2, *b = vsp - 1;

    return vtop >= 2
        && (val_const(a->kind) || a->kind == VAL_WIDE)
        && (val_const(b->kind) || b->kind == VAL_WIDE);
}

/* A constant of any kind as the bits `to` would hold: a narrow one widens by
 * its own sign, a long long narrows to a long's four bytes, and an integer
 * becoming a float is arithmetic, not a relabelling. */
static uint64_t const_as(const Value *v, Type to)
{
    uint64_t bits;
    int from_float = type_float(v->type), to_float = type_float(to);

    if (v->kind == VAL_WIDE) {
        bits = wide_value(v);

        /* A long is kept as its four bytes, so one on its way to eight takes
         * its sign with it -- or `long long x = -1405039608` is 2889927688,
         * the literal being a long because it does not fit in an int. */
        if (type_wide_bytes(v->type) == 4 && !type_float(v->type)
            && !type_unsigned(v->type))
            bits = (uint64_t) (int64_t) (int32_t) (uint32_t) bits;
    } else if (type_unsigned(v->type)) {
        /* The stack holds a narrow constant the way the machine would, with
         * its top bit run up through the host's word: 0x800000u is kept as
         * the same bits a 24-bit int of that value has, which read as a
         * number is negative. Widening it has to stop at its own width, or
         * the sign it does not have arrives in the bytes above -- and
         * `unsigned long x = 0x800000u` came out 0xff800000. */
        int n = type_size(v->type);

        bits = (uint64_t) (int64_t) v->val;
        if (n < 8)
            bits &= ((uint64_t) 1 << (8 * n)) - 1;
    } else {
        bits = (uint64_t) (int64_t) v->val;
    }

    if (to_float && !from_float) {
        int negative = !type_unsigned(v->type)
                       && wide_signed(bits, type_wide_bytes(v->type)) < 0;
        /* Taken away from zero as bits, which is also right for the most
         * negative long long -- negating it as a signed number is not. */
        uint64_t magnitude = negative
                             ? (uint64_t) 0
                               - (uint64_t) wide_signed(bits,
                                                        type_wide_bytes(v->type))
                             : bits;

        return float_from_int(magnitude, negative);
    }
    if (from_float && !to_float)
        return (uint64_t) float_to_int((uint32_t) bits);

    if (type_wide_bytes(to) == 4)
        bits &= 0xffffffffu;

    return bits;
}

/* A wide constant on the stack, if there is room in the table for it. */
static int wide_push(uint64_t bits, Type type, int ext)
{
    if (nwide_consts == WIDE_CONSTS)
        return 0;
    wide_consts[nwide_consts] = bits;
    vpush(VAL_WIDE, type, nwide_consts);
    (vsp - 1)->ext = (unsigned char) ext;
    nwide_consts++;

    return 1;
}

int vconst_wide(uint64_t *bits, Type *type)
{
    const Value *top = vsp - 1;

    if (vtop == 0)
        return 0;
    if (top->kind == VAL_WIDE) {
        *bits = wide_value(top);
        *type = top->type;

        return 1;
    }
    if (val_const(top->kind)) {       /* a narrow one widens here */
        *bits = type_unsigned(top->type) ? (uint64_t) (uint32_t) top->val
                                         : (uint64_t) (int64_t) top->val;
        *type = top->type;

        return 1;
    }

    return 0;
}

/* Whether the top is a constant that is an address inside the image -- the
 * question a global's initial value asks once it has been folded, because
 * what goes into the image then is bytes, and nothing about bytes says one
 * of them is the start of an address. */
int vconst_addr(void)
{
    return vtop > 0 && (vsp - 1)->kind == VAL_ADDR;
}

int vconst_bss(void)
{
    return vtop > 0 && (vsp - 1)->kind == VAL_BSS;
}

int vconst_top(int *val, Type *type)
{
    const Value *top = vsp - 1;

    if (vtop == 0 || !val_const(top->kind))
        return 0;
    *val = top->val;
    *type = top->type;

    return 1;
}

void vdrop(void)
{
    if (vtop == 0)
        acc_error("internal: value stack underflow");
    vtop--;
    vsp--;
}

/* The scratch area is free again at a statement boundary, and only there.
 *
 * It used to reset whenever the value stack emptied, on the reasoning that an
 * empty stack is a statement boundary. That stopped being true when longs
 * arrived: a long lives in the scratch area, and an operation on two of them
 * drops both operands -- emptying the stack -- and then pushes a result that
 * points into it. The next allocation started from zero and handed out the
 * slot the result was sitting in, so `a + b == c` compared c with itself.
 *
 * Caught by breaking the four-byte add to three and finding that a test still
 * passed: the comparison was not comparing what it looked like it was. */
void gen_stmt_end(void)
{
    spill_used = 0;
    spill_locked = 0;
    nspill_pending = 0;
    nwide_consts = 0;
}

/* Always inlined, for the same reason: the allocator asks it about every
 * register it considers, and the frame cost more than the loop. */
static inline __attribute__((always_inline))
int reg_busy(int reg)
{
    /* Walked as a pointer, not subscripted. `vstack[i]` is `vstack + i *
     * sizeof (Value)`, and scaling a variable is a call into the runtime on
     * this target whatever the width -- `__ishl` when it is a power of two
     * and `__imulu` when it is not. Walking costs an add of a constant
     * instead, and this and the three scans below are worth 1.5% of a
     * compile between them.
     *
     * vsp is `vstack + vtop` and is kept in step with it, so the end of the
     * walk is a pointer compare -- and an unsigned one, since a signed `<`
     * is another helper call. */
    const Value *v;

    for (v = vstack; v < vsp; v++)
        if (v->kind == VAL_REG && v->val == reg)
            return 1;

    return 0;
}

/* Frees a register by moving whatever is in it to a fresh frame slot. The
 * oldest is chosen because it is the one least likely to be wanted next: the
 * expression being compiled is working at the top of the stack.
 *
 * Never `avoid`, which is the register the caller is about to refuse:
 * freeing that one frees nothing it can use, and with every other register
 * busy the compiler then stopped with "no register after spilling". */
static void spill_one_other(int avoid)
{
    Value *v;

    for (v = vstack; v < vsp; v++) {
        if (v->kind == VAL_REG && v->val != avoid) {
            int off = spill_slot();

            ld_ix_rr(off, v->val);
            v->kind = VAL_LOCAL;
            v->val = off;

            return;
        }
    }
    acc_error("internal: nothing to spill");
}

/* Spills every register-held value except the top n, which the caller is
 * about to consume. */
static void save_regs_below(int n)
{
    Value *v, *end = vsp - n;

    for (v = vstack; v < end; v++) {
        if (v->kind == VAL_REG) {
            int off = spill_slot();

            ld_ix_rr(off, v->val);
            v->kind = VAL_LOCAL;
            v->val = off;
        }
    }
}

static void spill_one(void)
{
    spill_one_other(-1);
}

static int reg_alloc(void)
{
    int reg;

    for (reg = 0; reg < NREGS; reg++)
        if (!reg_busy(reg))
            return reg;
    spill_one();
    for (reg = 0; reg < NREGS; reg++)
        if (!reg_busy(reg))
            return reg;
    acc_error("internal: no register after spilling");

    return 0;
}

/* A register that is free and is not `avoid`, spilling to make one if need
 * be. */
static int reg_alloc_other(int avoid)
{
    int reg;

    for (reg = 0; reg < NREGS; reg++)
        if (reg != avoid && !reg_busy(reg))
            return reg;
    spill_one_other(avoid);
    for (reg = 0; reg < NREGS; reg++)
        if (reg != avoid && !reg_busy(reg))
            return reg;
    acc_error("internal: no register after spilling");

    return 0;
}

/* Materialises the entry at `depth` below the top into a register and returns
 * it. Anything already in a register stays where it is. */
/* The value at `val` -- the top of the stack or the one under it -- in a
 * register, and which one.
 *
 * Given the Value itself rather than a depth. `vsp - 1 - depth` inside the
 * function was a negation and a multiply by the size of a Value, both calls
 * into the runtime on this target, for every value forced into a register;
 * `vsp - 1` at the call site is a fixed offset, which the eZ80 does with one
 * lea. */
static int force_reg(Value *val)
{
    int reg;

    /* Forcing a value into a register is asking for it as an integer. For a
     * float that is a conversion, not a load of its low three bytes. */
    /* A struct is refused here too, with the same compare: its code is one
     * below float's, so the two are one range. */
    if ((Type) (val->type - TY_STRUCT) < 2u) {
        if (type_is_struct(val->type))
            struct_used();
        acc_error_at(tok_line, "converting a floating-point value to an "
                               "integer is not implemented yet");
    }

    if (val->kind == VAL_REG)
        return val->val;

    if (val->kind == VAL_ACC) {
        /* Widen out of A. This is the escape hatch that makes the byte path
         * safe: anything that does not understand VAL_ACC forces a register
         * and gets the promoted value, which is what C says it should see.
         *
         * Into HL when HL is free. When it is not, whatever lives there stays
         * and the byte goes to another register: a comparison puts its left
         * operand in HL and then asks for the right, and a left operand moved
         * out of HL to make room was lost -- `--w > 0` subtracted w from
         * itself. force_into does the widening without disturbing HL. */
        if (reg_owner(R_HL, val)) {
            reg = reg_alloc_other(R_HL);
            force_into(val, reg);

            return reg;
        }
        reg = R_HL;
        if (type_unsigned(val->type))
            fill_hl_with_zero();
        else
            fill_hl_with_sign_of_a();
        ld_l_a();
        val->kind = VAL_REG;
        val->type = type_promote(val->type);
        val->val = reg;

        return reg;
    }

    reg = reg_alloc();
    if (val_const(val->kind)) {
        if (val->kind == VAL_ADDR)
            out_reloc(out_here() + 1);
        else if (val->kind == VAL_BSS)
            gen_bss_fixup(out_here() + 1);
        ld_rr_imm(reg, val->val);
    } else if (val->kind == VAL_WIDE) {
        ld_rr_imm(reg, (int) (wide_value(val) & 0xffffff));
    } else {
        if (val->kind == VAL_VOID)
            void_used();
        if (type_size(val->type) < ACC_INT_SIZE)
            load_narrow_into(reg, val->val, val->type);
        else
            ld_rr_ix(reg, val->val);
    }
    val->kind = VAL_REG;
    val->type = type_promote(val->type);
    val->val = reg;

    return reg;
}

/* A register that is free and is not `avoid`, or -1: nothing is spilled to
 * find one. */
static int free_reg_other(int avoid)
{
    int reg;

    for (reg = 0; reg < NREGS; reg++)
        if (reg != avoid && !reg_busy(reg))
            return reg;

    return -1;
}

/* `entry`, which is in a register someone else needs, out of it: to another
 * register if one is free, and otherwise to the frame.
 *
 * Only this entry moves. Making room by spilling whichever value is oldest,
 * as this once did, could spill a value the caller had just put in place --
 * the left operand of the operator being applied, forced into HL a moment
 * before the right was forced into BC -- and the operator then read what had
 * been moved into HL instead. */
static void move_out(Value *entry)
{
    int to = free_reg_other(entry->val);

    if (to >= 0) {
        mov_rr(to, entry->val);
        entry->val = to;

        return;
    }

    to = spill_slot();
    ld_ix_rr(to, entry->val);
    entry->kind = VAL_LOCAL;
    entry->val = to;
}

/* Moves every value out of `reg`, so that it can be written without losing
 * anything the expression still needs. */
static void evict_reg(int reg)
{
    Value *v;

    for (v = vstack; v < vsp; v++)
        if (v->kind == VAL_REG && v->val == reg)
            move_out(v);
}

/* Materialises the entry at `depth` into one particular register, moving
 * whatever else is living there out of the way first. */
/* Named by pointer rather than by depth from the top. Every caller says
 * `vsp - 1` or `vsp - 2`, which is a constant offset; a depth has to be
 * turned into an address, and scaling an index is a helper call here. */
/* Whether a value other than `except` is living in a register. */
static int reg_owner(int reg, const Value *except)
{
    const Value *entry;

    for (entry = vstack; entry < vsp; entry++)
        if (entry != except && entry->kind == VAL_REG && entry->val == reg)
            return 1;

    return 0;
}

static void force_into(Value *target, int want)
{
    Value *entry;

    if (type_is_struct(target->type))
        struct_used();

    for (entry = vstack; entry < vsp; entry++) {
        if (entry == target)
            continue;
        if (entry->kind == VAL_REG && entry->val == want)
            move_out(entry);
    }

    if (target->kind == VAL_ACC) {
        /* A byte in A is widened through HL, whatever register it is going
         * to. Whatever lives in HL is kept there rather than moved away: the
         * callers put a left operand in HL and then place the right, and a
         * left operand moved to DE behind their backs was lost -- in
         * `x = 1 | x << 1` the OR was handed x << 1 twice, and in
         * `--w > 0` the comparison subtracted w from itself. */
        int keep = want != R_HL && reg_owner(R_HL, target);

        if (keep)
            push_rr(R_HL);
        if (type_unsigned(target->type))
            fill_hl_with_zero();
        else
            fill_hl_with_sign_of_a();
        ld_l_a();
        if (want != R_HL)
            mov_rr(want, R_HL);
        if (keep)
            pop_rr(R_HL);
    } else if (target->kind == VAL_REG) {
        if (target->val != want)
            mov_rr(want, target->val);
    } else if (val_const(target->kind)) {
        if (target->kind == VAL_ADDR)
            out_reloc(out_here() + 1);
        else if (target->kind == VAL_BSS)
            gen_bss_fixup(out_here() + 1);
        ld_rr_imm(want, target->val);
    } else if (target->kind == VAL_WIDE) {
        ld_rr_imm(want, (int) (wide_value(target) & 0xffffff));
    } else {
        if (target->kind == VAL_VOID)
            void_used();
        if (type_size(target->type) < ACC_INT_SIZE)
            load_narrow_into(want, target->val, target->type);
        else
            ld_rr_ix(want, target->val);
    }
    target->kind = VAL_REG;
    target->type = type_promote(target->type);
    target->val = want;
}

int vpop_reg(void)
{
    int reg = force_reg(vsp - 1);

    vdrop();

    return reg;
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
static int trunc_int(int value)
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

/* Whether an operator's operands are read as unsigned: either of them for
 * arithmetic, and only the left for a shift, whose count's type C99 6.5.7
 * says nothing about the result's. */
static int fold_unsigned(int op, const Value *lhs, const Value *rhs)
{
    if (tok_pair(op, TK_SHL))
        return type_unsigned(type_promote(lhs->type)) != 0;

    return either_unsigned(lhs, rhs);
}

static int in_function;                 /* see gen_func_begin */

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

static void vbinop(int op)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;
    int folded, right;
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

    /* Adding or subtracting nothing is nothing. Worth the two lines: it is
     * what makes `p + 0` and the zero cases of generated code free. */
    if (val_number(rhs->kind) && rhs->val == 0
        && (op == TK_PLUS || op == TK_MINUS)) {
        vdrop();

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

    /* add hl, rr and sbc hl, rr only accumulate into HL, so the left operand
     * goes there and the right one goes anywhere else. Both are done through
     * the allocator rather than by moving registers about by hand: a scratch
     * register chosen without asking whether anything already lives in it is
     * how `f(a,b,c) + f(1,2,3)` lost an argument. */
    force_into(vsp - 2, R_HL);

    /* A left shift by a constant of up to eight is add hl, hl a bit at a
     * time, a byte each: no bigger than loading the count into BC and
     * calling, and without the helper's loop. crc16 in test/perf's crc.c
     * shifts by 1 eight times a byte and by 8 once, and those two calls
     * were a third of its inner loop. A right shift cannot be done this
     * way: nothing shifts the third byte of HL right. */
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
    case TK_AMP:   rt_call(RT_AND); break;
    case TK_PIPE:  rt_call(RT_OR);  break;
    case TK_CARET: rt_call(RT_XOR); break;
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

    vdrop();
    vdrop();
    vpush_reg(R_HL);
    (vsp - 1)->type = result;
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
static int conversion_from = -1;
static int conversion_to;

/* Drop a value that nothing will read: a statement's, or the left side of a
 * comma. When it is still the one an assignment to a narrow object just
 * converted, the conversion goes too. */
void gen_discard(void)
{
    if (conversion_from >= 0 && out_here() == conversion_to
        && (vsp - 1)->kind == VAL_REG && (vsp - 1)->val == R_HL) {
        out_rewind(conversion_from);
        jumps_forget(conversion_from);
    }
    conversion_from = -1;
    vdrop();
}

void vstore_local(int offset, Type type)
{
    gen_effects++;
    int reg;

    /* An assignment converts the value to the type of the object, and
     * between a float and an integer that is arithmetic rather than a
     * relabelling. vconvert is where it lives; this used to refuse instead,
     * which is why a float could be stored and read back but never made from
     * anything. */
    if (type_float(type) != type_float((vsp - 1)->type) || type == TY_BOOL)
        vconvert(type);         /* a _Bool from all of a long, not its low
                                 * bytes, which the narrow store takes */

    if (type_wide(type)) {
        materialise_long(offset, type);
        vdrop();
        vpush(VAL_LOCAL, type, offset);

        return;
    }

    if ((vsp - 1)->kind == VAL_ACC && (vsp - 1)->type == type) {
        /* Already in A at its own width, which is where a byte store reads
         * from. Nothing to convert and nothing to move. */
        ld_ix_a(offset);

        return;
    }

    if (type_size(type) < ACC_INT_SIZE) {
        /* The narrow stores write out of HL, so the value goes there. They
         * write only the low bytes, which the conversion does not change,
         * so the store comes first and the conversion after, for the value
         * the assignment has -- which a statement throws away, and
         * gen_discard with it. */
        force_into(vsp - 1, R_HL);
        store_narrow(offset, type);
        conversion_from = out_here();
        vconvert(type);
        conversion_to = out_here();

        return;
    }

    reg = force_reg(vsp - 1);
    ld_ix_rr(offset, reg);
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
/* functions                                                           */

/* Calls to functions that have not been compiled yet.
 *
 * A one-pass compiler cannot know where a function will be until it reads it,
 * so the call is emitted with a hole and the hole is remembered. There are as
 * many of these as there are forward calls, which is nothing beside keeping
 * the whole program in memory to make two passes over it. */
typedef struct {
    int fn;                 /* index, not a pointer: see sym.c */
    int at;
    int line;               /* where the call was, for the diagnostic below */
    unsigned char declared; /* whether the call knew the function's type */
} Fixup;

static Fixup *fixups;
static int    nfixups, fixups_cap;

/* Whatever is being compiled has just named a function: see want(). */
static void want(int fn);

static void fixup_add(int fn, int at)
{
    want(fn);
    if (nfixups == fixups_cap) {
        fixups_cap = fixups_cap ? fixups_cap * 2 : 32;
        fixups = realloc(fixups, fixups_cap * sizeof *fixups);
        if (!fixups)
            acc_error("out of memory for forward calls");
    }
    out_reloc(at);
    fixups[nfixups].fn = fn;
    fixups[nfixups].at = at;
    fixups[nfixups].line = tok_line;
    fixups[nfixups].declared = (unsigned char) (sym_flags(fn) & SYMF_DECLARED);
    nfixups++;
}

/* The uses of `sym` so far, filled in now that it has an address rather
 * than at the end of the file: a block's static, used in its own
 * initialiser, is dropped with the rest of the block's names when its
 * function ends, and gen_finish would find nothing there. The relocation
 * each slot needs was recorded when the use was. */
void gen_settle(int sym)
{
    int i, kept = 0;

    for (i = 0; i < nfixups; i++) {
        if (fixups[i].fn != sym) {
            fixups[kept++] = fixups[i];
            continue;
        }
        out_patch24(fixups[i].at, sym_at(sym)->val + out_read24(fixups[i].at));
    }
    nfixups = kept;
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

static void ld_a_imm(int value)  { out_byte2(0x3e, value & 0xff); }
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

/* jp cc, nn -- the condition codes this file uses. */
#define JP_ANY  0xc3
#define JP_Z    0xca
#define JP_NZ   0xc2
#define JP_PE   0xea            /* overflow */
#define JP_P    0xf2            /* sign clear */
#define JP_M    0xfa            /* sign set */
#define JP_C    0xda            /* carry set: a borrow, so unsigned less */
#define JP_NC   0xd2            /* and clear, so unsigned not less */

static int jump_op(int op);
static void patch_to_here(int hole);

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
static int cmp_from = -1;       /* where the making of the value began */
static int cmp_to;              /* and where it ended */
static int cmp_op;              /* the comparison it was */
static int cmp_was_unsigned;

/* The same mark for an AND with a mask that keeps one byte of the value and
 * nothing else: the flags the AND left say whether that byte, and so the
 * whole answer, is zero -- until the sbc hl, hl that clears the rest, which
 * is where `from` stands. `if (c & 0x8000)` then comes to the AND and a
 * jump on its zero flag. */
static void flags_say_nonzero(int from)
{
    cmp_from = from;
    cmp_to = out_here();
    cmp_op = TK_NE;
    cmp_was_unsigned = 1;
}


/* The equality half: Z is the whole answer. `when_equal` is what to leave
 * when the two were equal, which is 1 for `==` and 0 for `!=`. */
static void cmp_equal(int when_equal)
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
static void cmp_unsigned(int when_borrow)
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
static void cmp_signed(int when_negative)
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

static void vcmp(int op)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;
    int folded, right, is_unsigned;

    if ((unsigned) vtop < 2)
        acc_error("internal: a comparison with nothing to compare");

    is_unsigned = either_unsigned(lhs, rhs);

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
static void cmp_value(int op, int is_unsigned)
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
    vpush_reg(R_HL);
}

/* ------------------------------------------------------------------ */
/* long                                                                */

/* A long is four bytes and every register is three, so a long value never
 * lives in one. It stays in the frame, and the operations work on it there:
 * HL points at the destination, DE at the other operand, and a helper walks
 * the four bytes. That is the shape the chip is good at for something wider
 * than a register, and it reuses the scratch area the allocator already has.
 *
 * A long long is the same shape with eight bytes instead of four, and a long
 * double with eight it does no arithmetic on: everything here takes the
 * width from the type rather than assuming a long's, and the routine it
 * calls is the eight-byte one. type_wide_bytes is that width.
 *
 * On the stack a wide value is therefore always VAL_LOCAL. Anything that
 * forces it into a register is asking for the int it converts to, which is
 * its low three bytes -- so force_reg does exactly that and needs no special
 * case.
 */

static int spill_slot_of(int size);

/* lea rr, ix+d -- the address of a frame slot, which is what the helpers take.
 * The second byte is the register, and these are the assembler's own numbers
 * rather than a reading of the opcode map: guessing DE cost a debugging pass. */
static void lea_rr_ix(int reg, int disp)
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
        for (i = 0; i < n; i++) {               /* one of them is out of reach */
            ld_a_ix(from + i);
            ld_ix_a(to + i);
        }

        return;
    }
    for (i = 0; i < n; i++) {
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
static void convert_int_to_float(void)
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

static void convert_float_to_int(Type to)
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
static void no_float_address(const Value *from)
{
    if (val_pending(from->kind))
        acc_error_at(tok_line, "an address cannot become a floating-point "
                               "value");
}

/* The bytes of a float and of an integer mean different things, so moving a
 * value between them is arithmetic and not a copy. Every path that widens or
 * stores four bytes comes through here, which is why the check lives here
 * rather than at each of them. */
static void check_no_float_mix(Type to, const Value *from)
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
static void materialise_long(int disp, Type type)
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

    for (i = 0; i < 8; i++)
        bytes[i] = (unsigned char) (bits >> (i * 8));
#else
    const unsigned char *bytes = (const unsigned char *) &bits;
#endif

    for (i = 0; i < n; i++) {
        out_byte(0x3e);                         /* ld a, n */
        out_byte(bytes[i]);
        ld_ix_a(disp + i);
    }
}

/* And a slot of its own for it, which is what a wide constant becomes when
 * something needs it where a wide value lives. */
static void wide_to_slot(uint64_t bits, Type type)
{
    int slot = spill_slot_of(type_wide_bytes(type));

    wide_bytes_at(slot, bits, type_wide_bytes(type));
    vpush_scratch(type, slot);
}

/* One scratch slot as wide as the type, for the left operand of an operation
 * to be built in and overwritten by the result. */
static int long_scratch(Type type)
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
static int spill_start_of(const Value *v, int *size)
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
static int64_t wide_signed(uint64_t bits, int width)
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
static void vunary_long(int which, Type type)
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

static void vbinop_long(int op, Type result)
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
    int low, rstart, in_place;

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
    rstart = spill_used > low + n ? spill_used : low + n;

    /* The right operand is built first, because building the left one may
     * need HL and the right may still be an expression on the stack. The
     * left goes where the answer is to be, which is at or below where the
     * operands are. */
    left = slot_at(low, n);
    if (in_place) {
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

    materialise_long(left, result);
    vdrop();

    lea_rr_ix(R_HL, left);
    lea_rr_ix(R_DE, right);
    rt_call(which);

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
static void vcmp_wide(int op, Type operand)
{
    int floating = type_float(operand);
    int n = type_wide_bytes(operand);
    int low, rstart, in_place;
    int left, right;

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

    /* As in vbinop_long: the integer comparisons only read through DE, so a
     * right operand already in the frame is compared where it lies. The
     * float one rewrites both operands and cannot. */
    in_place = !floating && wide_in_place(vsp - 1, operand, n);
    low = spill_lowest(2, in_place);
    rstart = spill_used > low + n ? spill_used : low + n;

    left = slot_at(low, n);
    if (in_place) {
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
    }
    lea_rr_ix(R_HL, left);
    lea_rr_ix(R_DE, right);

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
static int in_function;

static int           *jump_at, *jump_put;
static unsigned char *jump_cc, *jump_cc_put;
static int            njumps, jumps_cap;

static void jumps_rewind(int n)
{
    njumps = n;
    jump_put = jump_at + n;
    jump_cc_put = jump_cc + n;
}

static int jump_op(int op)
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
static void jumps_forget(int here)
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
    while (hole) {
        unsigned char *at = out_img + (hole - out_base);
        int next = get24(at);

        put24(at, target);
        hole = next;
    }
}

static void patch_to_here(int hole)
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

static int jump_on_truth(int when_true)
{
    int reg;

    if (cmp_from >= 0 && out_here() == cmp_to && vtop == 1
        && (vsp - 1)->kind == VAL_REG && (vsp - 1)->val == R_HL) {
        int op = when_true ? cmp_op : cmp_opposite(cmp_op);

        out_rewind(cmp_from);
        jumps_forget(cmp_from);
        cmp_from = -1;
        vdrop();

        return jump_on_flags(op, cmp_was_unsigned);
    }

    if (type_wide(vtype()))
        vtruth(TK_NE);

    reg = vpop_reg();
    if (reg != R_HL)
        mov_rr(R_HL, reg);

    /* There is no "is this register zero" instruction for a 24-bit value.
     * The upper byte of HL is not addressable, so the 16-bit idiom -- `ld a,l`
     * then `or a,h` -- would test two thirds of the value and call 0x010000
     * false. Subtracting zero tests all of it. */
    ld_rr_imm(R_BC, 0);
    or_a_a();
    sbc_hl_rr(R_BC);

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
static void bool_from(void)
{
    vtruth(TK_NE);
}

void vtruth(int op)
{
    /* Straight after a comparison, or an AND that left its mark, the flags
     * are still there to be read: `!(a < b)` is a >= b, and `!(c & 0x80)`
     * is the AND's zero flag, with no compare against zero in between. */
    if (cmp_from >= 0 && out_here() == cmp_to
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
    int late, done;

    save_regs_below(1);
    late = jump_on_truth(settles);

    ld_rr_imm(R_HL, !settles);          /* neither side settled it */
    done = jump_op(JP_ANY);

    patch_to_here(early);
    patch_to_here(late);
    ld_rr_imm(R_HL, settles);           /* one of them did */

    patch_to_here(done);
    vpush_reg(R_HL);
}

void gen_label(int hole)
{
    patch_to_here(hole);
}

/* ------------------------------------------------------------------ */
/* the runtime helpers                                                 */

/* The operations the chip has no instruction for. acc has nothing to link
 * against, so it carries them and drops the ones a program uses into that
 * program's image -- see src/rt/helpers.s, which is where they are written
 * and read. A program that uses none pays nothing.
 *
 * Calls to them are recorded like calls to a function defined further down
 * the file, because that is what they are: the address is not known until the
 * end, when the runtime is laid out after the last function.
 *
 * It goes in whole rather than a routine at a time, because the routines
 * share code -- the four ways of dividing are one loop with four ways in --
 * and splitting them would mean four copies of that loop. A program that uses
 * any of them carries all of them, which at a few hundred bytes against the
 * Agon's 448 KB is the cheaper trade.
 *
 * With one cut, at RT_SPLIT: the eight-byte routines are another 800 bytes
 * and nothing above the cut calls anything below it, so a program that never
 * uses a long long does not carry them. */
static int rt_base = 0;                 /* where the blob landed */

/* How much of the blob a program wants: 1 for what talks to the machine, 2
 * for the arithmetic as well, 3 for the long long routines too. The blob is
 * laid out in that order and nothing above a cut calls below it, so what is
 * emitted is always a run from its first byte. */
static int rt_any_used;

typedef struct {
    unsigned char which;
    int at;
} RtFixup;

static RtFixup *rt_fixups;
static int      nrt_fixups, rt_fixups_cap;

/* The symbol each helper is known by, when something has had to name one:
 * SYM_NONE until then.
 *
 * A file compiled to an object does not carry the blob -- one copy of it per
 * object is one copy too many -- so a call to a helper leaves the object as a
 * call to `acc_rt_mul` and the like, and the link resolves them all against
 * the single copy it lays down. Which is what a helper always was: a function
 * the program calls and does not define. */
static int rt_syms[RT_COUNT];

static void rt_syms_init(void)
{
    int i;

    for (i = 0; i < RT_COUNT; i++)
        rt_syms[i] = SYM_NONE;
}

/* The helper of that name, or -1. Asked of every name a link cannot resolve,
 * which is a handful, so the names are walked rather than hashed. */
static int rt_which(const char *name)
{
    int i;

    for (i = 0; i < RT_COUNT; i++)
        if (strcmp(rt_name[i], name) == 0)
            return i;

    return -1;
}

/* That something wants this helper, whatever laid the want down: a call
 * emitted here, or a name that came in from an object. */
static void rt_wanted(int which)
{
    int want = rt_entry[which] >= RT_SPLIT ? 3
             : rt_entry[which] >= RT_OPS   ? 2 : 1;

    if (want > rt_any_used)
        rt_any_used = want;
}

/* Its symbol, made the first time one is asked for. Only at the end of a
 * compile, where pushing a file-scope symbol cannot move a Sym * that
 * something is holding. */
static int rt_symbol(int which)
{
    if (rt_syms[which] == SYM_NONE) {
        int sym = sym_push(name_intern(rt_name[which],
                                       (int) strlen(rt_name[which])),
                           SYM_FUNC, 0);

        sym_set_flags(sym, SYMF_DECLARED | SYMF_PARAMS);
        rt_syms[which] = sym;
    }

    return rt_syms[which];
}

/* The operators with no instruction behind them. */
static int needs_helper(int op)
{
    switch (op) {
    case TK_AMP: case TK_PIPE: case TK_CARET:
    case TK_SHL: case TK_SHR:
    case TK_STAR: case TK_SLASH: case TK_PERCENT:
        return 1;
    }

    return 0;
}

static void rt_call(int which)
{
    if (nrt_fixups == rt_fixups_cap) {
        rt_fixups_cap = rt_fixups_cap ? rt_fixups_cap * 2 : 16;
        rt_fixups = realloc(rt_fixups, rt_fixups_cap * sizeof *rt_fixups);
        if (!rt_fixups)
            acc_error("out of memory for the runtime fixups");
    }
    rt_wanted(which);
    out_opcode24(0xcd, 0);                       /* call nn */
    rt_fixups[nrt_fixups].which = (unsigned char) which;
    rt_fixups[nrt_fixups].at = out_here() - ACC_INT_SIZE;
    out_reloc(rt_fixups[nrt_fixups].at);
    nrt_fixups++;
}

/* The blob, once, wherever the image has got to -- which is after everything
 * else, since this is the last thing written. Every call to a helper is then
 * pointed at it: the ones this compile emitted directly, and the ones that
 * arrived as a name from an object. */
static void rt_emit_used(void)
{
    int i, len;

    if (!rt_any_used)
        return;

    len = rt_any_used == 3 ? (int) sizeof rt_code
        : rt_any_used == 2 ? RT_SPLIT : RT_OPS;
    rt_base = out_here();
    for (i = 0; i < len; i++)
        out_byte(rt_code[i]);

    /* The calls the routines make to each other, now that the blob has an
     * address -- those in the part that was laid down. */
    for (i = 0; i < RT_NFIX; i++)
        if (rt_fix[i].at < len) {
            out_reloc(rt_base + rt_fix[i].at);
            out_patch24(rt_base + rt_fix[i].at, rt_base + rt_fix[i].to);
        }

    /* And the calls the compiled program makes to them. */
    for (i = 0; i < nrt_fixups; i++)
        out_patch24(rt_fixups[i].at, rt_base + rt_entry[rt_fixups[i].which]);

    /* The ones that came in by name have a symbol, and the fixups waiting on
     * it are filled in with the rest of them below. */
    for (i = 0; i < RT_COUNT; i++)
        if (rt_syms[i] != SYM_NONE) {
            sym_at(rt_syms[i])->val = rt_base + rt_entry[i];
            sym_set_flags(rt_syms[i], SYMF_DEFINED);
        }
}

/* Calls this file cannot resolve, when it is being compiled to an object:
 * the function is defined somewhere else, and where the call has to point is
 * the linker's to work out. The slot keeps the zero it was emitted with,
 * which is the addend, and the relocation names the symbol.
 *
 * Kept apart from the relocation table rather than in it, because out.c does
 * not know about symbols and should not have to. The object writer puts the
 * two together. */
int gen_objects;                /* -c: compiling to an object */

typedef struct {
    int at, fn;
} ExternFix;

static ExternFix *externs;
static int        nexterns, externs_cap;

static void extern_add(int at, int fn)
{
    if (nexterns == externs_cap) {
        externs_cap = externs_cap ? externs_cap * 2 : 16;
        externs = realloc(externs, (size_t) externs_cap * sizeof *externs);
        if (!externs)
            acc_error("out of memory for the calls out of this file");
    }
    externs[nexterns].at = at;
    externs[nexterns].fn = fn;
    nexterns++;
}

int gen_nexterns(void)
{
    return nexterns;
}

int gen_extern_at(int i)
{
    return externs[i].at;
}

int gen_extern_sym(int i)
{
    return externs[i].fn;
}

/* ------------------------------------------------------------------ */
/* what starts at zero                                                 */

/* A variable C says starts at zero takes no room in the file. It is given an
 * address past the image's last byte instead, and the program clears what is
 * there before main runs. `int big[1000];` was three thousand bytes of zeros
 * to read off the card; now it is three thousand bytes of nothing.
 *
 * Where the bss starts is not known until the whole image has been written,
 * but where in it each variable goes is known as it is declared. So a use of
 * one is its offset, which folds like any other constant -- `a[3]` is one
 * load with the nine already in it -- and the slot is written down for the
 * start of the bss to be added to it at the end. One number, the same for
 * all of them, which is why these are a list of places and not fixups
 * against a symbol.
 *
 * That is also what lets a block's `static` live here. It has no name
 * outside its function and its symbol is dropped at the end of it, so there
 * would be nothing to hang a symbol on -- and it needs none.
 *
 * File-scope ones are written down as symbols as well, because another
 * object may want to name them. */
typedef struct {
    int sym, at;
} BssSym;

static BssSym *bss_syms;
static int     nbss_syms, bss_syms_cap;
static int     bss_len;
static int     bss_init_hole = -1;      /* the stub's call to the clearing */
static int     args_hole = -1;          /* and the one to the arguments */
static int     bss_extra;               /* room past it that is not cleared */
static const char *exec_name;           /* what is being built, for argv[0] */
static int     bss_top;                 /* the first byte past everything */

/* Slots holding an offset into the bss, which want the start of it added.
 *
 * The same shape as the calls into the runtime blob: a list of places,
 * settled in one pass once there is an address to settle them with. Not
 * fixups against a symbol, because there is no symbol -- every one of them
 * wants the same one number, and a block's `static` has no name outside the
 * function it is in to hang a symbol on. */
static int *bss_fixups;
static int  nbss_fixups, bss_fixups_cap;

/* Kept in order of where they are, as the relocation table is and for the
 * same reason: the object writer walks the two together to say which of the
 * slots it is writing down want the bss rather than the image. They nearly
 * always arrive in order -- a slot is recorded where it is emitted -- and a
 * global's initial values are the exception, since a designated one is
 * written out of order. */
void gen_bss_fixup(int at)
{
    int i;

    if (nbss_fixups == bss_fixups_cap) {
        bss_fixups_cap = bss_fixups_cap ? bss_fixups_cap * 2 : 32;
        bss_fixups = realloc(bss_fixups,
                             (size_t) bss_fixups_cap * sizeof *bss_fixups);
        if (!bss_fixups)
            acc_error("out of memory for the offsets into the bss");
    }
    out_reloc(at);
    for (i = nbss_fixups; i > 0 && bss_fixups[i - 1] > at; i--)
        bss_fixups[i] = bss_fixups[i - 1];
    bss_fixups[i] = at;
    nbss_fixups++;
}

int gen_nbss_fixups(void)
{
    return nbss_fixups;
}

int gen_bss_fixup_at(int i)
{
    return bss_fixups[i];
}

/* Room in the bss, and where in it. The linker asks for a whole object's
 * worth at once and hands out the pieces itself. */
/* The largest alignment anything in the bss asked for, as a log2: where
 * the bss starts is rounded up to it. An assembly object may ask for a
 * page-aligned buffer; nothing acc compiles does. */
static int bss_align;
static int bss_start;                   /* where bss_emit put it */

int gen_bss_reserve_aligned(int bytes, int align)
{
    int step = 1 << align;

    if (align > bss_align)
        bss_align = align;
    bss_len = (bss_len + step - 1) & ~(step - 1);

    return gen_bss_reserve(bytes);
}

int gen_bss_reserve(int bytes)
{
    int at = bss_len;

    if (bytes < 0 || bss_len + bytes < bss_len)
        acc_error("internal: %d bytes of bss", bytes);
    bss_len += bytes;

    return at;
}

void gen_bss_symbol(int sym, int at)
{
    if (nbss_syms == bss_syms_cap) {
        bss_syms_cap = bss_syms_cap ? bss_syms_cap * 2 : 16;
        bss_syms = realloc(bss_syms, (size_t) bss_syms_cap * sizeof *bss_syms);
        if (!bss_syms)
            acc_error("out of memory for the variables that start at zero");
    }
    bss_syms[nbss_syms].sym = sym;
    bss_syms[nbss_syms].at = at;
    nbss_syms++;
}

/* Where in the bss a symbol is, or -1 when it is not there. Asked by the
 * object writer, of every symbol it exports, so that the object can say
 * which of them have room in the file and which want it cleared. */
int gen_bss_offset(int sym)
{
    int i;

    for (i = 0; i < nbss_syms; i++)
        if (bss_syms[i].sym == sym)
            return bss_syms[i].at;

    return -1;
}

int gen_bss_len(void)
{
    return bss_len;
}

/* Whether anything has already been compiled that reaches into [at, at +
 * bytes] of the bss.
 *
 * Asked when a variable that was given room there turns out, further down
 * the file, to have a value after all: its bytes then go in the file and the
 * room here is abandoned, which is fine -- unless something in between was
 * compiled to look in the room. The range is closed at both ends on purpose:
 * the address one past the end of an array is a real thing to have taken,
 * and it is not worth being clever about whose it is. */
/* A variable that was in the bss, at `at` for `bytes`, and has been given
 * room in the image at `to` instead -- `int n;`, used, and then `int n =
 * 30;`. Every slot written so far with an offset into its bytes, or just
 * past them, is an address of it: each becomes that address in the image,
 * relocated as the image's are -- which is what a slot not on the bss's
 * list is -- and is no longer one the bss's start is added to. */
void gen_bss_move(int at, int bytes, int to)
{
    int i, kept = 0;

    for (i = 0; i < nbss_fixups; i++) {
        int slot = bss_fixups[i], was = out_read24(slot);

        if (was < at || was > at + bytes) {
            bss_fixups[kept++] = slot;
            continue;
        }
        out_patch24(slot, to + was - at);       /* relocated already: see */
    }                                           /* gen_bss_fixup */
    nbss_fixups = kept;
}

/* And that it is no longer there, so that the addresses handed out at the
 * end leave it alone. What it was given stays reserved: a hole in an area
 * that is all zeros anyway. */
void gen_bss_forget(int sym)
{
    int i;

    for (i = 0; i < nbss_syms; i++)
        if (bss_syms[i].sym == sym) {
            bss_syms[i] = bss_syms[--nbss_syms];

            return;
        }
}

/* The routine that clears the bss, and the addresses of everything in it.
 *
 * Last of all, so that what follows the image's last byte is the bss itself
 * -- which is the whole point: the file stops, and the zeros do not have to
 * be in it. The routine is in the file, just before them.
 *
 * It has three shapes, and which one follows from how much there is to
 * clear, so its own length is known before it is written and with it where
 * the bss starts. One byte is the shape that has to be told apart: `ld (hl),
 * 0` has already cleared it, and an ldir of the nothing left over would be
 * an ldir of bc = 0, which on this chip is not nothing but sixteen
 * megabytes -- it cleared the machine out from under the program. */
#define BSS_INIT_ONE  7                 /* ld hl, base / ld (hl), 0 / ret */
#define BSS_INIT_LEN  17                /* and an ldir along the rest */

static void bss_emit(void)
{
    int base, i;

    if (bss_init_hole < 0)
        return;                         /* no entry stub: nothing calls it */

    out_patch24(bss_init_hole, out_here());
    if (!bss_len) {
        out_byte(0xc9);                 /* ret */
        base = out_here();
    } else {
        /* It starts past the routine that clears it, rounded up to the most
         * any of it asked to be aligned to. The bytes between are outside
         * the image: the bss is room, and nothing is written into it. */
        base = out_here() + (bss_len == 1 ? BSS_INIT_ONE : BSS_INIT_LEN);
        base = (base + (1 << bss_align) - 1) & ~((1 << bss_align) - 1);
        out_byte(0x21);                         /* ld hl, base */
        out_reloc(out_here());
        out_word24(base);
        out_byte2(0x36, 0x00);                  /* ld (hl), 0 */
        if (bss_len > 1) {
            out_byte(0x11);                     /* ld de, base + 1 */
            out_reloc(out_here());
            out_word24(base + 1);
            out_byte(0x01);                     /* ld bc, bss_len - 1 */
            out_word24(bss_len - 1);
            out_byte2(0xed, 0xb0);              /* ldir */
        }
        out_byte(0xc9);                         /* ret */

        for (i = 0; i < nbss_syms; i++) {
            sym_at(bss_syms[i].sym)->val = base + bss_syms[i].at;
            sym_set_flags(bss_syms[i].sym, SYMF_DEFINED);
        }
    }

    if (base - out_base + bss_len + bss_extra > ACC_RAM_BYTES)
        acc_error("the program and what it leaves at zero come to %d bytes, "
                  "and the Agon has %d for both",
                  base - out_base + bss_len + bss_extra, ACC_RAM_BYTES);

    /* Every slot that holds an offset into it, which is where the folding
     * went: `a[3]` put nine there, and this makes it an address. The table
     * the argument routine keeps is one of them, and it is there whether the
     * program left anything at zero or not -- so this runs even when there
     * was nothing to clear. */
    for (i = 0; i < nbss_fixups; i++)
        out_patch24(bss_fixups[i], out_read24(bss_fixups[i]) + base);

    bss_top = base + bss_len + bss_extra;
    bss_start = base;
}

/* argc and argv, out of what MOS passed.
 *
 * MOS enters a program with HL pointing at the rest of the command line --
 * everything after the name that was typed, which is why the name is not in
 * it and has to come from somewhere else. The routine below walks that text,
 * writes a zero over each separator so that every word is a string of its
 * own, and fills a table of pointers to them. It is the same walk agondev's
 * startup makes, down to the sixteen-argument limit, because a program that
 * works under one has to work under the other.
 *
 * It is written whether main asks for it or not. Whether it does is not a
 * question the link can answer -- main arrives from an object, and an object
 * says what its symbols are called and not what they take -- and a program
 * that works compiled in one piece and not linked from two would be a worse
 * thing than the ninety bytes. A main that takes no parameters is handed two
 * values it never reads.
 *
 * The bytes come from src/rt/startup.s, as the stub's do. */
#define ARGV_MAX  16            /* as agondev's startup has it */

static const unsigned char args_code[] = {
    0xdd, 0x21, 0x00, 0x00, 0x00,               /* ld ix, argv */
    0x01, 0x00, 0x00, 0x00,                     /* ld bc, the name */
    0xdd, 0x0f, 0x00,                           /* ld (ix+0), bc */
    0xed, 0x32, 0x03,                           /* lea ix, ix+3 */
    0xcd, 0x00, 0x00, 0x00,                     /* call spaces */
    0x0e, 0x01, 0x06, ARGV_MAX,                 /* ld c, 1 / ld b, 16 */
    0xc5, 0xe5,                                 /* next: push bc / push hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call token */
    0x79, 0xd1, 0xc1,                           /* ld a, c / pop de / pop bc */
    0xb7, 0x28, 0x13,                           /* or a, a / jr z, done */
    0xdd, 0x1f, 0x00,                           /* ld (ix+0), de */
    0xe5, 0xd1,                                 /* push hl / pop de */
    0xcd, 0x00, 0x00, 0x00,                     /* call spaces */
    0xaf, 0x12,                                 /* xor a, a / ld (de), a */
    0xed, 0x32, 0x03,                           /* lea ix, ix+3 */
    0x0c, 0x79, 0xb8, 0x38, 0xe1,               /* inc c / cp b / jr c, next */
    0x11, 0x00, 0x00, 0x00, 0x59,               /* done: ld de, 0 / ld e, c */
    0x21, 0x00, 0x00, 0x00,                     /* ld hl, argv */
    0xc9,
    0x0e, 0x00,                                 /* token: ld c, 0 */
    0x7e, 0xb7, 0xc8,                           /* ld a, (hl) / ret z */
    0xfe, 0x0d, 0xc8,                           /* cp 13 / ret z */
    0xfe, 0x20, 0xc8,                           /* cp ' ' / ret z */
    0x23, 0x0c, 0x18, 0xf3,                     /* inc hl / inc c / jr */
    0x7e, 0xfe, 0x20, 0xc0,                     /* spaces: ld a,(hl) / ret */
    0x23, 0x18, 0xf9                            /* inc hl / jr */
};

/* Its holes, as offsets from its first byte: the table of pointers, which is
 * in the bss and so wants the bss added; the name, which is in the image;
 * and the table again, where it is answered with. */
#define ARGS_ARGV_AT    0x02
#define ARGS_NAME_AT    0x06
#define ARGS_ARGV2_AT   0x3c

static const struct { int at, to; } args_calls[] = {
    { 0x10, 0x4f },                             /* spaces */
    { 0x1a, 0x40 },                             /* token */
    { 0x29, 0x4f }                              /* spaces */
};

/* The name argv[0] is given. MOS does not pass one -- what was typed is not
 * in the text it hands over -- so this is the name of the file being built,
 * which is the nearest thing to the truth that is known here. agondev puts a
 * fixed string there instead. */
static const char *base_name(const char *path)
{
    const char *at = path, *p;

    for (p = path; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':')
            at = p + 1;

    return at;
}

static void args_emit(void)
{
    const char *name;
    int base, argv_at, name_at, i;

    if (args_hole < 0)
        return;                         /* no entry stub: nothing calls it */

    out_patch24(args_hole, out_here());

    /* The table goes past everything that is cleared rather than among it.
     * Nothing reads a slot the routine has not written -- argc says how many
     * there are -- so zeroing forty-eight bytes at every start would be work
     * for no one, and leaving them out keeps a program whose own bss is one
     * byte a program whose bss is one byte. */
    argv_at = bss_len;
    bss_extra = ARGV_MAX * ACC_INT_SIZE;
    base = out_here();
    for (i = 0; i < (int) sizeof args_code; i++)
        out_byte(args_code[i]);

    name = base_name(exec_name ? exec_name : "");
    name_at = out_here();
    for (i = 0; name[i]; i++)
        out_byte((unsigned char) name[i]);
    out_byte(0);

    out_patch24(base + ARGS_NAME_AT, name_at);
    out_reloc(base + ARGS_NAME_AT);
    out_patch24(base + ARGS_ARGV_AT, argv_at);
    gen_bss_fixup(base + ARGS_ARGV_AT);
    out_patch24(base + ARGS_ARGV2_AT, argv_at);
    gen_bss_fixup(base + ARGS_ARGV2_AT);

    for (i = 0; i < (int) (sizeof args_calls / sizeof *args_calls); i++) {
        out_patch24(base + args_calls[i].at, base + args_calls[i].to);
        out_reloc(base + args_calls[i].at);
    }
}

/* Two names the link answers for, because only it knows them: where the
 * program's own memory ends and where the machine's does. What is between
 * them is nobody's, which is what makes it a heap -- and the stack comes
 * down from above it, so the top is kept clear of where the stack will be.
 *
 * A program that never asks for them costs nothing for their being here:
 * they are only ever looked for among the names nothing has defined.
 *
 * The stub asks for a third, the top of the program's memory, where main's
 * stack starts. That one is known by its symbol rather than its name. */
static int stack_top_sym = -1;   /* made by gen_finish, for the stub */
static int stack_top_at;         /* where in the stub it goes */

static int link_given(int sym)
{
    const char *name;

    if (sym == stack_top_sym)
        return out_base + ACC_RAM_BYTES;
    name = name_text(sym_at(sym)->name);

    if (strcmp(name, "acc_heap_start") == 0)
        return bss_top;
    if (strcmp(name, "acc_heap_end") == 0)
        return out_base + ACC_RAM_BYTES - ACC_STACK_RESERVE;

    return 0;
}

/* A symbol the fixups are waiting on that nothing here has given an address
 * to. A function has one once its body has been read -- asked of the flag
 * and not of the address, because an object puts its first function at
 * offset zero. A variable has one once room has been reserved for it, which
 * a declaration that only says extern does not do: that leaves -1 behind. */
/* A slot of any kind, filled with a value that is known: see the note on
 * relocation kinds in src/obj.c. A PCREL8's value is where it jumps to. */
void gen_slot(int at, int kind, long value)
{
    long d;

    switch (kind) {
    case REL_ABS24:  out_patch24(at, (int) value);                  break;
    case REL_LOW8:   out_patch8(at, (int) (value & 0xff));          break;
    case REL_HIGH8:  out_patch8(at, (int) ((value >> 8) & 0xff));   break;
    case REL_UPPER8: out_patch8(at, (int) ((value >> 16) & 0xff));  break;
    case REL_ABS16:  out_patch16(at, (int) (value & 0xffff));       break;
    default:
        d = value - (at + 1);
        if (d < -128 || d > 127)
            acc_error("a relative jump at %06x reaches %ld bytes, and one "
                      "can reach 127 forward and 128 back", at, d);
        out_patch8(at, (int) (d & 0xff));
        break;
    }
}

/* Slots that want a symbol's address, or the bss's, and are not the three
 * bytes gen_data_fixup fills: the kinds an assembly object has. Filled once
 * everything has an address, at the end of gen_finish. */
typedef struct {
    int  sym, at, kind;
    long addend;
} LateFixup;

static LateFixup *late;
static int        nlate, late_cap;

void gen_late_fixup(int sym, int at, int kind, long addend)
{
    if (nlate == late_cap) {
        late_cap = late_cap ? late_cap * 2 : 16;
        late = realloc(late, (size_t) late_cap * sizeof *late);
        if (!late)
            acc_error("out of memory for the link's slots");
    }
    late[nlate].sym = sym;
    late[nlate].at = at;
    late[nlate].kind = kind;
    late[nlate].addend = addend;
    nlate++;
}

int gen_nlate(void)
{
    return nlate;
}

int gen_late_sym(int i)
{
    return late[i].sym;
}

static int no_address(int sym);

static void late_fill(int bss_base)
{
    int i;

    for (i = 0; i < nlate; i++) {
        long target;

        if (late[i].sym < 0) {
            target = bss_base;
        } else {
            Sym *s = sym_at(late[i].sym);

            if (no_address(late[i].sym) && !name_weak(s->name))
                acc_error("'%s' is used but never defined", name_text(s->name));
            target = no_address(late[i].sym) ? 0 : s->val;
        }
        gen_slot(late[i].at, late[i].kind, target + late[i].addend);
    }
}

static int no_address(int sym)
{
    return sym_at(sym)->kind == SYM_FUNC
           ? !(sym_flags(sym) & SYMF_DEFINED)
           : sym_at(sym)->val < 0;
}

/* The calls and addresses still waiting on something, which is how a link
 * knows what to go looking for in a library. */
int gen_nfixups(void)
{
    return nfixups;
}

int gen_fixup_sym(int i)
{
    return fixups[i].fn;
}

int gen_no_address(int sym)
{
    return no_address(sym);
}

/* The file's own functions, and the run of bytes each of them is.
 *
 * A `static` at file scope is this file's alone, so if nothing in the file
 * wanted it, nothing ever can -- and it does not have to be in the image.
 * That matters more than it sounds: a header full of `static inline`
 * helpers is compiled into every file that includes it, whether that file
 * calls them or not, and acc does not inline. Six files of zap include one
 * such header and were carrying fifteen kilobytes of it each.
 *
 * They cannot be left out as they are read -- a call may come later in the
 * file -- so they are written like any other function and taken out again at
 * the end, which is what the runs below are for. */
typedef struct {
    int sym, at, len;
} StaticFn;

static StaticFn *static_fns;
static int       nstatic_fns, static_fns_cap, static_now = -1;

/* One of them holding another's address: which one said it, and which one it
 * named. Recorded where the reference is made, because that is the one place
 * both are known without looking. Read back out of the image afterwards, it
 * was a search of the relocation table for every one of them, and the search
 * cost more than the pass saved: on this chip an index into a table is a
 * multiply and the length of one is a divide, and both are calls.
 *
 * A reference from anywhere else -- a function the whole program can name, a
 * global's bytes -- is not recorded at all. It is marked on the symbol as it
 * is made, and that mark is what a walk of these starts from. */
typedef struct {
    int from, sym;
} Want;

static Want *wants;
static int   nwants, wants_cap;

static void want(int fn)
{
    if (static_now < 0 || !(sym_flags(fn) & SYMF_STATIC)) {
        sym_set_flags(fn, SYMF_USED);

        return;
    }
    if (nwants == wants_cap) {
        wants_cap = wants_cap ? wants_cap * 2 : 64;
        wants = realloc(wants, (size_t) wants_cap * sizeof *wants);
        if (!wants)
            acc_error("out of memory for what the file's functions want");
    }
    wants[nwants].from = static_now;
    wants[nwants].sym = fn;
    nwants++;
}

static void static_begin(int fn)
{
    static_now = -1;
    if (!(sym_flags(fn) & SYMF_STATIC))
        return;

    if (nstatic_fns == static_fns_cap) {
        static_fns_cap = static_fns_cap ? static_fns_cap * 2 : 32;
        static_fns = realloc(static_fns,
                             (size_t) static_fns_cap * sizeof *static_fns);
        if (!static_fns)
            acc_error("out of memory for the file's own functions");
    }
    static_now = nstatic_fns++;
    static_fns[static_now].sym = fn;
    static_fns[static_now].at = out_here();
}

static void static_end(void)
{
    if (static_now >= 0)
        static_fns[static_now].len = out_here() - static_fns[static_now].at;
    static_now = -1;
}

/* The file's own functions that nothing in it wanted, taken back out.
 *
 * Every address acc has written down is in one of four places: the
 * relocation table, which names each slot in the image holding one; the
 * fixups, the runtime's and the bss's, which name slots that hold something
 * else until gen_finish fills them in; and the symbols. So the walk is over
 * those, and nothing else has to be told.
 *
 * It runs before anything is laid down at the end of the image -- the
 * runtime blob, the argument routine, the clearing -- so that what those are
 * placed at is the address they will keep, and so that the bss and the heap
 * begin as far down as the shortened image allows. */
/* Where in the list a symbol's function is, or -1 for one that is not the
 * file's own, through the file's own functions in the order of the symbols
 * that name them.
 *
 * static_fns is in the order the functions were defined; a symbol's number
 * is the order it was first named. A forward declaration names one function
 * before a later line defines another, and from then on the two orders
 * disagree -- so searching the definition order as though it were sorted by
 * symbol found nothing. What it could not find it took to be a function
 * outside the file, and the call stayed while the function it called was
 * dropped. That is how aed's cmd_ops.c stopped compiling.
 *
 * Built once, before the marking, and insertion-sorted because the two
 * orders agree except where a forward declaration parts them: the walk is
 * the whole of it for a file that declares nothing ahead of itself. */
static int *by_sym, by_sym_cap;

static void sort_by_sym(void)
{
    int i;

    if (nstatic_fns > by_sym_cap) {
        by_sym_cap = nstatic_fns * 2;
        by_sym = realloc(by_sym, (size_t) by_sym_cap * sizeof *by_sym);
        if (!by_sym)
            acc_error("out of memory ordering the file's own functions");
    }
    for (i = 0; i < nstatic_fns; i++) {
        int at = static_fns[i].sym, j = i;

        while (j > 0 && static_fns[by_sym[j - 1]].sym > at) {
            by_sym[j] = by_sym[j - 1];
            j--;
        }
        by_sym[j] = i;
    }
}

static int static_index(int sym)
{
    int lo = 0, hi = nstatic_fns - 1;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int at = static_fns[by_sym[mid]].sym;

        if (sym < at)
            hi = mid - 1;
        else if (sym > at)
            lo = mid + 1;
        else
            return by_sym[mid];
    }

    return -1;
}

/* Which of the file's own functions anything that is staying still wants.
 *
 * The ones something outside them named are marked already, and each of the
 * rest is wanted only if the function that named it is itself staying -- so
 * this is a walk out from those, and not a question that can be answered one
 * function at a time. Two helpers that call each other and nothing else are
 * wanted by nobody, however loudly they say otherwise.
 *
 * The references are bucketed by who made them first, so that the walk is
 * one step per reference rather than a pass over all of them per function.
 * A function cannot want itself: the edge is not recorded when it would.
 */
static void mark_live(unsigned char *live)
{
    int *head, *next, *stack, top = 0, i;

    for (i = 0; i < nstatic_fns; i++)
        live[i] = (sym_flags(static_fns[i].sym) & SYMF_USED) != 0;
    if (!nwants)
        return;
    sort_by_sym();

    head = malloc((size_t) nstatic_fns * sizeof *head);
    next = malloc((size_t) nwants * sizeof *next);
    stack = malloc((size_t) nstatic_fns * sizeof *stack);
    if (!head || !next || !stack)
        acc_error("out of memory taking the unused functions out");

    for (i = 0; i < nstatic_fns; i++)
        head[i] = -1;
    for (i = 0; i < nwants; i++) {
        next[i] = head[wants[i].from];
        head[wants[i].from] = i;
    }

    for (i = 0; i < nstatic_fns; i++)
        if (live[i])
            stack[top++] = i;

    while (top) {
        int from = stack[--top], e;

        for (e = head[from]; e >= 0; e = next[e]) {
            int to = static_index(wants[e].sym);

            if (to < 0 || live[to])
                continue;
            live[to] = 1;
            stack[top++] = to;
        }
    }

    free(head);
    free(next);
    free(stack);
}

static int dead_statics(Cut *cuts, const unsigned char *live)
{
    int i, n = 0;

    for (i = 0; i < nstatic_fns; i++) {
        if (live[i] || !static_fns[i].len)
            continue;
        cuts[n].at = static_fns[i].at;
        cuts[n].len = static_fns[i].len;
        n++;
    }

    return n;
}

/* Where each of the lists acc keeps stood when a function began, so that
 * shortening that function's jumps walks its own entries and not the ones
 * every function before it left. Everything recorded before the first run is
 * where it was. */
typedef struct {
    int reloc, fixup, rt, bss, jump;
} Mark;

static Mark func_mark;          /* where the lists stood at this function */

static void mark_here(Mark *m)
{
    m->reloc = out_nrelocs();
    m->fixup = nfixups;
    m->rt = nrt_fixups;
    m->bss = nbss_fixups;
    m->jump = njumps;
}

/* The slots that do not hold an address yet, gathered and put in order.
 *
 * A call waiting on a function that is not defined holds the amount to add
 * to that function's address, one waiting on the runtime holds nothing at
 * all, and one waiting on the bss holds an offset into it. All three are
 * filled in after this pass has run, so what is in them now is not an
 * address and must not be moved as if it were.
 *
 * Only what was recorded since the function began, when it is a function's
 * jumps being shortened: everything before that is where it was, and
 * gathering it again for every function is the whole file's worth of it once
 * per function.
 *
 * The three lists are each in the order they were written, which is rising,
 * so they are merged rather than sorted: a qsort over them cost three
 * quarters of a million cycles on the Agon, which is more than the pass it
 * belongs to. The one exception is a variable said twice at file scope,
 * whose value is written back over the room the first mention reserved --
 * that puts a slot behind ones already recorded, and fixup_add says so. The
 * merge is followed by an insertion pass, which is a walk when the merge
 * came out in order and a repair when it did not. */
static int *pending, pending_cap;

static int *pending_slots(int *count, const Mark *from)
{
    int n = (nfixups - from->fixup) + (nrt_fixups - from->rt)
          + (nbss_fixups - from->bss);
    int *all, *put, *b, *b_end, *scan;
    const Fixup *f, *f_end;
    const RtFixup *r, *r_end;

    /* Nothing is waiting, which is the usual answer once each function is
     * asked about its own tail: most of them have no call left to fill in
     * and nothing in the bss. Said here rather than left to the loops
     * because the buffer is not there yet the first time through, and one
     * past the start of a buffer that is not there is not a place. */
    *count = 0;
    if (n <= 0)
        return pending;

    b = bss_fixups + from->bss;
    b_end = bss_fixups + nbss_fixups;
    f = fixups + from->fixup;
    f_end = fixups + nfixups;
    r = rt_fixups + from->rt;
    r_end = rt_fixups + nrt_fixups;

    /* Grown and kept: see the note in out_cut_sum. */
    if (n > pending_cap) {
        pending_cap = n * 2;
        pending = realloc(pending, (size_t) pending_cap * sizeof *pending);
        if (!pending)
            acc_error("out of memory taking the unused functions out");
    }
    all = put = pending;

    /* Walked with pointers and not indexed: an index into an array of
     * anything three bytes wide is a multiply, and a multiply is a call. */
    while (f < f_end || r < r_end || b < b_end) {
        int x = f < f_end ? f->at : INT_MAX;
        int y = r < r_end ? r->at : INT_MAX;
        int z = b < b_end ? *b : INT_MAX;

        if (x <= y && x <= z)
            *put++ = f++->at;
        else if (y <= z)
            *put++ = r++->at;
        else
            *put++ = *b++;
    }

    for (scan = all + 1; scan < put; scan++) {
        int at = *scan, *back = scan;

        while (back > all && back[-1] > at) {
            back[0] = back[-1];
            back--;
        }
        *back = at;
    }
    *count = (int) (put - all);

    return all;
}

/* A list of positions, with what is gone taken out of it. */
static int cut_positions(int *at, int n)
{
    int *from = at, *put = at, i;

    out_cut_rewind();
    for (i = 0; i < n; i++, from++) {
        int to = out_cut_next(*from);

        if (to >= 0)
            *put++ = to;
    }

    return (int) (put - at);
}

/* Runs of bytes taken out of the image, and everything acc knows about
 * where things are brought along.
 *
 * Two passes want this: the one that leaves out the functions a file does
 * not use, and the one that makes a jump two bytes when its target is near.
 * What they cut differs; what has to be told afterwards does not.
 *
 * `holes` says whether a run may be pointed into. It may not, for a function
 * that is going: an address inside one is a reference to something nothing
 * was said to want, which is a mistake in the marking rather than something
 * to carry on from. It may, for a jump being shortened: the run is the
 * middle of its own operand, and the slot that holds it goes with it. */
static void cut_out(Cut *cuts, int ncuts, int holes, const Mark *from)
{
    int *slots, *p, *seen_at, npending, seen = 0, i;

    out_cut_sum(cuts, ncuts);
    slots = pending_slots(&npending, from);
    seen_at = slots;

    /* What the slots hold, while they are still where they were written.
     *
     * The relocations are in order and so is the list of slots to leave
     * alone, so telling them apart is one step through that list per slot. */
    out_cut_rewind();
    for (p = out_relocs + 1 + from->reloc; p < out_reloc_put; p++) {
        int slot = out_base + *p, to;

        while (seen < npending && *seen_at < slot)
            seen++, seen_at++;
        if (seen < npending && *seen_at == slot)
            continue;
        if (out_cut_next(slot) < 0)
            continue;                   /* going away with the code it is in */
        to = out_cut_moved(get24(out_img + *p));
        if (to < 0) {
            if (holes)
                continue;
            acc_error("internal: %06x holds the address of a function "
                      "nothing was said to want", slot);
        }
        put24(out_img + *p, to);
    }

    out_cut(cuts, ncuts, from->reloc);

    /* And then every position and address acc is still holding. A fixup
     * inside a run goes with it: the call it was going to fill in is not
     * there any more, and neither is whatever it would have asked a library
     * for. */
    {
        Fixup *scan = fixups + from->fixup, *keep = scan;
        RtFixup *rscan = rt_fixups + from->rt, *rkeep = rscan;

        out_cut_rewind();
        for (; scan < fixups + nfixups; scan++) {
            int to = out_cut_next(scan->at);

            if (to < 0)
                continue;
            *keep = *scan;
            keep->at = to;
            keep++;
        }
        nfixups = (int) (keep - fixups);

        out_cut_rewind();
        for (; rscan < rt_fixups + nrt_fixups; rscan++) {
            int to = out_cut_next(rscan->at);

            if (to < 0)
                continue;
            *rkeep = *rscan;
            rkeep->at = to;
            rkeep++;
        }
        nrt_fixups = (int) (rkeep - rt_fixups);
    }

    nbss_fixups = from->bss
                + cut_positions(bss_fixups + from->bss, nbss_fixups - from->bss);

    /* The jumps are not brought along here. relax_function is the only
     * caller that has any, and it works out where each one landed from the
     * runs directly -- they and the jumps are both in rising order, so that
     * is a running total and not a lookup, and it saves a third walk of a
     * list that is as long as the function has jumps. */

    if (holes)
        return;                 /* a jump shortened moves nothing outside it */

    /* How much of the runtime blob is wanted, asked again of what is left:
     * a function that has gone is not multiplying anything, and the blob is
     * laid down in one piece up to the furthest routine any call in the
     * program reaches. */
    rt_any_used = 0;
    for (i = 0; i < nrt_fixups; i++)
        rt_wanted(rt_fixups[i].which);

    for (i = 0; i < sym_nglobals(); i += (int) sizeof(Sym)) {
        Sym *sym = sym_at(i);
        int to;

        /* Only what is at an address in this image: a constant's value is a
         * value, and a variable waiting for the bss holds an offset that is
         * negative until bss_emit turns it into one. */
        if (sym->val < 0 || sym->kind == SYM_CONST || sym->kind == SYM_TYPEDEF)
            continue;
        if (sym->kind == SYM_FUNC && !(sym_flags(i) & SYMF_DEFINED))
            continue;
        to = out_cut_moved(sym->val);
        if (to >= 0)
            sym->val = to;
        else if (sym->kind == SYM_FUNC)
            sym->val = -1;      /* one of the ones taken out: it is nowhere */
    }
}

/* The `jr` that says the same thing as a `jp`, or zero where there is none.
 * This chip has a relative jump for the four conditions the flags register
 * answers directly and none for the three a signed comparison needs. */
/* Whether a distance fits in the one signed byte a `jr` carries.
 *
 * Written as one unsigned compare rather than two signed ones, because a
 * signed compare is a call into the runtime on this chip and an unsigned
 * one is not -- the same reason out_reloc compares offsets unsigned. This
 * is asked twice for every jump the compiler writes. */
#define JR_REACHES(d) ((unsigned) ((d) + 128) <= 255u)

static int jr_of(int op)
{
    switch (op) {
    case JP_ANY: return 0x18;
    case JP_Z:   return 0x28;
    case JP_NZ:  return 0x20;
    case JP_C:   return 0x38;
    case JP_NC:  return 0x30;
    }

    return 0;
}

/* The jumps of the function just compiled whose target is near enough,
 * written again in two bytes.
 *
 * Done as each function ends rather than once at the end of the file, and
 * that is the whole of why it is affordable. Nothing outside a function
 * points into it -- C has no way to name a place inside one -- and nothing
 * after it has been written yet, so taking bytes out of it moves nothing
 * that is already down. What has to be brought back is the function's own
 * relocations, its own fixups and its own jumps, and each of those lists is
 * walked from where it stood when the function began.
 *
 * Done at the end of the file instead, every one of those lists had to be
 * walked whole, and every address in the image looked up in a list of
 * thousands of runs: it cost between a fifth and a half of the time it takes
 * to compile, against the four percent of the image it saves.
 *
 * The two bytes taken out are the middle of the jump's own operand, so the
 * opcode stays where it is and one byte of distance is left behind it.
 *
 * Nothing here looks a jump up in the relocation table, because a jump is
 * not in it yet: jump_op leaves the operand unrecorded and this says where
 * each surviving one ended up, in one rising run that out_reloc_merge puts
 * in among the few relocations a function makes for anything else. That is
 * what the pass costs and what it used to cost: walking the table meant
 * walking every jump a second time, and the table is nearly all jumps. */
static Cut           *relax_cuts;
static int           *relax_target, *relax_slot;
static unsigned char *relax_short;
static int            relax_cap;

static void relax_function(const Mark *from)
{
    Cut *cuts;
    int *target, *slot, *put;
    unsigned char *shrink;
    int n = njumps - from->jump, ncuts = 0, nslots = 0, i;

    if (n <= 0)
        return;

    /* Grown and kept, as everything on this path is: a function is a handful
     * of jumps and there are hundreds of functions. */
    if (n > relax_cap) {
        relax_cap = n * 2;
        relax_cuts = realloc(relax_cuts, (size_t) relax_cap * sizeof *relax_cuts);
        relax_target = realloc(relax_target,
                               (size_t) relax_cap * sizeof *relax_target);
        relax_slot = realloc(relax_slot,
                             (size_t) relax_cap * sizeof *relax_slot);
        relax_short = realloc(relax_short, (size_t) relax_cap);
        if (!relax_cuts || !relax_target || !relax_slot || !relax_short)
            acc_error("out of memory shortening the jumps");
    }
    cuts = relax_cuts;
    target = relax_target;
    slot = relax_slot;
    shrink = relax_short;

    /* Where each jump goes, and which of them will reach in one byte.
     *
     * Decided against the function as it stands, before any of them have
     * shrunk. That is the conservative answer and needs no second thought:
     * taking bytes out from between a jump and its target only brings the
     * two closer, whichever way round they are. Some that just miss would
     * come into reach once their neighbours shrink, and they are left. */
    {
        const int *at = jump_at + from->jump;
        const unsigned char *cc = jump_cc + from->jump;
        unsigned char *fits = shrink;
        Cut *cut = cuts;

        put = target;
        for (i = 0; i < n; i++, at++, cc++) {
            int to = get24(out_img + (*at + 1 - out_base));
            int d = to - (*at + 2);

            *put++ = to;
            *fits++ = jr_of(*cc) && JR_REACHES(d);
            if (!fits[-1])
                continue;
            cut->at = *at + 1;
            cut->len = 2;
            cut++;
            ncuts++;
        }
    }

    if (ncuts)
        cut_out(cuts, ncuts, 1, from);

    /* Each jump written where it now is, and the ones still four bytes wide
     * handed to the relocation table.
     *
     * Both of those come from a running total rather than from asking. The
     * runs are the jumps' own operands, so walking the two together in
     * rising order says how much has gone before each jump; a jump's own run
     * begins a byte after the jump does, so it is never counted into its own
     * position. And a target is a step or two from there either way -- a
     * jump that is being shortened reaches 127 bytes and no further, and one
     * that is not has a target that has moved by whatever its neighbours
     * did -- so the same cursor answers for it, walked forward or back.
     * Looking each target up among the runs instead was the single most
     * expensive thing this pass did.
     *
     * A target is never inside a run: a run is the middle of a jump's
     * operand and a label is at an instruction, so there is no case here for
     * an address that does not land anywhere. */
    {
        const int *here = jump_at + from->jump, *want = target;
        const unsigned char *cc = jump_cc + from->jump, *fits = shrink;
        const Cut *run = cuts, *run_end = cuts + ncuts;
        int gone = 0;

        put = slot;
        for (i = 0; i < n; i++, here++, cc++, want++, fits++) {
            const Cut *r;
            unsigned char *at;
            int now, to, g;

            while (run < run_end && (unsigned) run->at <= (unsigned) *here) {
                gone += run->len;
                run++;
            }
            now = *here - gone;

            r = run;
            g = gone;
            if ((unsigned) *want > (unsigned) *here)
                while (r < run_end && (unsigned) r->at <= (unsigned) *want) {
                    g += r->len;
                    r++;
                }
            else
                while (r > cuts && (unsigned) r[-1].at > (unsigned) *want) {
                    r--;
                    g -= r->len;
                }
            to = *want - g;
            at = out_img + (now - out_base);

            if (*fits) {
                int d = to - (now + 2);

                if (!JR_REACHES(d))
                    acc_error("internal: a jump at %06x reaches %d, which is "
                              "further than it was", now, d);
                at[0] = (unsigned char) jr_of(*cc);
                at[1] = (unsigned char) d;

                continue;
            }
            put24(at + 1, to);
            *put++ = now + 1 - out_base;
            nslots++;      /* counted, not measured: the difference of two
                            * int pointers is a divide here */
        }
    }

    out_reloc_merge(slot, nslots, from->reloc);

    /* The function is written; what its jumps were is nobody's business now. */
    jumps_rewind(from->jump);
}

static void drop_unused_statics(void)
{
    unsigned char *live;
    Cut *cuts;
    int ncuts;

    if (!nstatic_fns)
        return;

    cuts = malloc((size_t) nstatic_fns * sizeof *cuts);
    live = calloc((size_t) nstatic_fns, 1);
    if (!cuts || !live)
        acc_error("out of memory taking the unused functions out");
    mark_live(live);
    ncuts = dead_statics(cuts, live);
    free(live);
    if (ncuts) {
        Mark start;

        start.reloc = start.fixup = start.rt = start.bss = start.jump = 0;
        cut_out(cuts, ncuts, 0, &start);
    }
    free(cuts);
}

void gen_finish(void)
{
    int i;

#ifndef ACC_NODROP
    drop_unused_statics();
#endif

    /* A helper wanted by name, which is how one arrives from an object: that
     * object used it and did not carry the blob. Claimed before anything is
     * laid down, so that the blob knows how much of itself to be. */
    if (!gen_objects)
        for (i = 0; i < nfixups; i++) {
            int which;

            if (!no_address(fixups[i].fn))
                continue;
            which = rt_which(name_text(sym_at(fixups[i].fn)->name));
            if (which < 0)
                continue;
            rt_syms[which] = fixups[i].fn;
            rt_wanted(which);
        }

    /* Compiling to an object, a call to a helper is a call to a name, and
     * the blob stays here: one copy of it goes into the program that is
     * linked, rather than one into every object that multiplies. */
    if (gen_objects) {
        for (i = 0; i < nrt_fixups; i++)
            extern_add(rt_fixups[i].at, rt_symbol(rt_fixups[i].which));
    } else {
        rt_emit_used();
        args_emit();
        bss_emit();

        /* The stub's stack top, a name made here rather than with the
         * stub: made there, it was interned ahead of every name in the
         * source and moved all of them, and the macro table hashes by
         * where a name is -- which cost text.c 13,000 cycles in collisions
         * for nothing. Made after the helpers are claimed, it is not
         * compared against every helper's name either. */
        stack_top_sym = sym_push(name_intern("acc_stack_top", 13),
                                 SYM_GLOBAL, -1);
        sym_set_flags(stack_top_sym, SYMF_DECLARED | SYMF_EXTERN);
        fixup_add(stack_top_sym, stack_top_at);

        /* And the ones the link itself answers for, now that there is an
         * answer: everything else has been laid down. */
        for (i = 0; i < nfixups; i++) {
            int sym = fixups[i].fn, at;

            if (!no_address(sym) || (at = link_given(sym)) == 0)
                continue;
            sym_at(sym)->val = at;
            sym_set_flags(sym, SYMF_DEFINED);
        }
    }

    for (i = 0; i < nfixups; i++) {
        Sym *fn = sym_at(fixups[i].fn);

        if (no_address(fixups[i].fn)) {
            if (gen_objects) {
                extern_add(fixups[i].at, fixups[i].fn);
                continue;
            }
        }

        /* A weak reference nothing else wanted is at zero, and so is what
         * reads it as a pointer: see do_pragma. */
        if (no_address(fixups[i].fn) && name_weak(fn->name)) {
            fn->val = 0;
            if (fn->kind == SYM_FUNC)
                sym_set_flags(fixups[i].fn, SYMF_DEFINED);
        }
        if (no_address(fixups[i].fn)) {
            if (fn->kind == SYM_FUNC)
                acc_error("'%s' is called but never defined",
                          name_text(fn->name));
            acc_error("'%s' is declared and used but never given room, so "
                      "another file has to define it -- which needs the "
                      "pieces linked together", name_text(fn->name));
        }

        /* The call was emitted before the definition was read, so it took C's
         * word that an undeclared function returns int -- and read its answer
         * from HL. A one-byte return comes back in A instead, which that call
         * cannot know. There are no prototypes yet, so the only honest thing
         * is to say so. */
        /* And a void one conflicts with that int outright, as agondev
         * says: it is the same function declared two ways. A call that had
         * a prototype to go by knew the type, and read the answer from
         * where it is. */
        /* Plus whatever the slot was emitted with, which is nothing for a
         * call or a register load and is the amount added for an address in
         * a global's bytes: `int *p = &g + 1` puts the one there, because
         * where g is was not known when the bytes were written. */
        if (fixups[i].declared) {
            out_patch24(fixups[i].at, fn->val + out_read24(fixups[i].at));
            continue;
        }
        if (fn->type == TY_VOID)
            acc_error_at(fixups[i].line,
                         "'%s' returns void and is called before it is "
                         "defined, which declares it as returning int; move "
                         "its definition above the call", name_text(fn->name));
        if (RETURNS_IN_A(fn->type))
            acc_error_at(fixups[i].line,
                         "'%s' returns a one-byte type and is called before it "
                         "is defined; move its definition above the call",
                         name_text(fn->name));
        out_patch24(fixups[i].at, fn->val + out_read24(fixups[i].at));
    }
    late_fill(bss_start);
}


void gen_init(void)
{
    vtop = 0;
    vsp = vstack;
    rt_syms_init();
}

/* The first thing in the image, because MOS enters at its first byte.
 *
 * The bytes come from src/rt/startup.s, assembled and copied in rather than
 * hand-encoded, so that the source of truth is assembly anyone can read and
 * reassemble. Three versions:
 *
 *   return  calls main and returns its result to MOS in hl, printing
 *           nothing, as agondev's startup does. The default: a program
 *           leaves the screen as it found it.
 *   print   calls main, writes the result as six hex digits and returns to
 *           MOS: -p, for reading an answer at a command prompt or on a real
 *           Agon.
 *   exit    calls main and hands the low byte to IO port 0, which stops the
 *           emulator with that byte as its exit status: -x, which is how the
 *           tests read an answer with no C library and nothing to print with.
 */
/* IY is saved and put back around the whole of the program, in every stub.
 *
 * MOS wants it as it left it: a program that returns having changed it takes
 * the machine down, and takes it down after the program has run and printed
 * its answer, which is as confusing a way to find this out as there is. It
 * cost a morning. The backend uses IY as its own scratch -- see the note on
 * the value stack -- so the program will have changed it, and the stub is
 * the one place that can put it back whatever the program did.
 *
 * In the exit stub too, though that one stops the machine rather than
 * returning: the contract is that a program acc compiles gives IY back, and
 * a contract with a hole in it for the mode the tests use is worse than no
 * contract. */
static const unsigned char startup_exit[] = {
    0xfd, 0xe5,                                 /* push iy */
    0xe5,                                       /* push hl: MOS's line */
    0xcd, 0x00, 0x00, 0x00,                     /* call the clearing */
    0xe1,                                       /* pop hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call the arguments */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (mos_sp), sp */
    0x31, 0x00, 0x00, 0x00,                     /* ld sp, the top */
    0xe5, 0xd5,                                 /* push hl / push de */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (exit_sp), sp */
    0x21, 0x00, 0x00, 0x00,                     /* ld hl, after main */
    0x22, 0x00, 0x00, 0x00,                     /* ld (exit_pc), hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call main */
    0xc1, 0xc1,                                 /* pop bc / pop bc */
    0xe5,                                       /* push hl: the status */
    0x2a, 0x00, 0x00, 0x00,                     /* ld hl, (exit hook) */
    0xcd, 0x00, 0x00, 0x00,                     /* call jp (hl) below */
    0xe1,                                       /* pop hl */
    0xed, 0x7b, 0x00, 0x00, 0x00,               /* ld sp, (mos_sp) */
    0x7d, 0xd3, 0x00,                           /* ld a, l / out (0), a */
    0xfd, 0xe1,                                 /* pop iy */
    0xc9,
    0xe9,                                       /* jp (hl) */
    0xc9                                        /* the hook if none */
};

static const unsigned char startup_return[] = {
    0xfd, 0xe5,                                 /* push iy */
    0xe5,                                       /* push hl: MOS's line */
    0xcd, 0x00, 0x00, 0x00,                     /* call the clearing */
    0xe1,                                       /* pop hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call the arguments */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (mos_sp), sp */
    0x31, 0x00, 0x00, 0x00,                     /* ld sp, the top */
    0xe5, 0xd5,                                 /* push hl / push de */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (exit_sp), sp */
    0x21, 0x00, 0x00, 0x00,                     /* ld hl, after main */
    0x22, 0x00, 0x00, 0x00,                     /* ld (exit_pc), hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call main */
    0xc1, 0xc1,                                 /* pop bc / pop bc */
    0xe5,                                       /* push hl: the status */
    0x2a, 0x00, 0x00, 0x00,                     /* ld hl, (exit hook) */
    0xcd, 0x00, 0x00, 0x00,                     /* call jp (hl) below */
    0xe1,                                       /* pop hl */
    0xed, 0x7b, 0x00, 0x00, 0x00,               /* ld sp, (mos_sp) */
    0xfd, 0xe1,                                 /* pop iy */
    0xc9,                                       /* ret, the status in hl */
    0xe9,                                       /* jp (hl) */
    0xc9                                        /* the hook if none */
};

static const unsigned char startup_print[] = {
    0xfd, 0xe5,                                 /* push iy */
    0xe5,                                       /* push hl: MOS's line */
    0xcd, 0x00, 0x00, 0x00,                     /* call the clearing */
    0xe1,                                       /* pop hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call the arguments */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (mos_sp), sp */
    0x31, 0x00, 0x00, 0x00,                     /* ld sp, the top */
    0xe5, 0xd5,                                 /* push hl / push de */
    0xed, 0x73, 0x00, 0x00, 0x00,               /* ld (exit_sp), sp */
    0x21, 0x00, 0x00, 0x00,                     /* ld hl, after main */
    0x22, 0x00, 0x00, 0x00,                     /* ld (exit_pc), hl */
    0xcd, 0x00, 0x00, 0x00,                     /* call main */
    0xc1, 0xc1,                                 /* pop bc / pop bc */
    0xe5,                                       /* push hl: the status */
    0x2a, 0x00, 0x00, 0x00,                     /* ld hl, (exit hook) */
    0xcd, 0x00, 0x00, 0x00,                     /* call jp (hl) below */
    0xe1,                                       /* pop hl */
    0xed, 0x7b, 0x00, 0x00, 0x00,               /* ld sp, (mos_sp) */
    0xe5,                                       /* push hl */
    0xfd, 0x21, 0x00, 0x00, 0x00, 0xfd, 0x39,   /* ld iy, 0 / add iy, sp */
    0xfd, 0x7e, 0x02, 0xcd, 0x00, 0x00, 0x00,   /* ld a, (iy+2) / hexbyte */
    0xfd, 0x7e, 0x01, 0xcd, 0x00, 0x00, 0x00,
    0xfd, 0x7e, 0x00, 0xcd, 0x00, 0x00, 0x00,
    0xe1,                                       /* pop hl */
    0x3e, 0x0d, 0x5b, 0xd7,                     /* ld a, 13 / rst.lil 0x10 */
    0x3e, 0x0a, 0x5b, 0xd7,                     /* ld a, 10 / rst.lil 0x10 */
    0xfd, 0xe1,                                 /* pop iy */
    0xc9,
    0xf5, 0x1f, 0x1f, 0x1f, 0x1f,               /* hexbyte: push af / rra x4 */
    0xcd, 0x00, 0x00, 0x00, 0xf1,               /* call hexnib / pop af */
    0xe6, 0x0f, 0xc6, 0x30, 0xfe, 0x3a,         /* hexnib: the digit */
    0x38, 0x02, 0xc6, 0x07, 0x5b, 0xd7, 0xc9,
    0xe9,                                       /* jp (hl) */
    0xc9                                        /* the hook if none */
};

/* The three holes in either stub, as offsets from its first byte: the call
 * into the routine that clears what starts at zero, the one that turns MOS's
 * command line into argc and argv, and the one into main. */
#define STUB_CLEAR_AT   4
#define STUB_ARGS_AT    9
/* Where exit comes back to, and where the stack was when main was called:
 * see gen_exit. The stub writes both before calling main, so that exit has
 * somewhere to go and a stack to go there on. */
#define STUB_SP_AT     25
#define STUB_PC_AT     29
#define STUB_PCSTORE_AT 33
#define STUB_MAIN_AT   37
#define STUB_AFTER_MAIN 40
/* The exit hook: the load of its cell, and the call to the `jp (hl)` that
 * each stub has as its last two bytes but one. */
#define STUB_HOOK_AT   44
#define STUB_HOOK_CALL_AT 48

/* The stack main runs on, which is not MOS's: see src/rt/startup.s. The stub
 * keeps MOS's stack pointer in a cell behind itself, moves to the top of the
 * program's memory, and puts MOS's back before it returns. */
#define STUB_MOS_SP_AT      14
#define STUB_TOP_AT         18
#define STUB_MOS_SP_BACK_AT 54

/* Where the print stub calls within itself, as offsets from its first byte.
 * They are absolute calls, so they have to be filled in once the stub's
 * address is known. */
static const struct { int at, to; } print_calls[] = {
    { 0x45, 0x62 }, { 0x4c, 0x62 }, { 0x53, 0x62 },   /* hexbyte */
    { 0x68, 0x6c }                                    /* hexnib */
};

/* The two cells the stub writes. See gen_startup and gen_exit.
 *
 * They are known by name, because a file compiled to an object has never
 * seen the stub: the link lays that down, and until it does there is no
 * saying where the cells are. So the object leaves a name behind and the
 * link answers it -- which is what a call to a helper does, for the same
 * reason.
 *
 * The names are acc's own. A program that spells one of them means this. */
static const char *const exit_cell_name[3] = {
    "__acc_exit_sp", "__acc_exit_pc", "__acc_exit_hook"
};

static int exit_cell_syms[3] = { SYM_NONE, SYM_NONE, SYM_NONE };

static int exit_cell(int which)
{
    if (exit_cell_syms[which] == SYM_NONE) {
        const char *name = exit_cell_name[which];
        int sym = sym_push(name_intern(name, (int) strlen(name)),
                           SYM_GLOBAL, -1);

        /* extern, so that the pass over the globals that gives a tentative
         * definition its room in the bss leaves these alone: gen_startup
         * puts them in the image instead, and a file compiled to an object
         * wants them left for the link to find. */
        sym_set_flags(sym, SYMF_DECLARED | SYMF_EXTERN);
        exit_cell_syms[which] = sym;
    }

    return exit_cell_syms[which];
}

void gen_startup(int ending, const char *program)
{
    int m = sym_push(name_intern("main", 4), SYM_FUNC, 0);
    const unsigned char *stub = ending == END_EXIT ? startup_exit
                                : ending == END_PRINT ? startup_print
                                : startup_return;
    int n = ending == END_EXIT ? (int) sizeof startup_exit
            : ending == END_PRINT ? (int) sizeof startup_print
            : (int) sizeof startup_return;
    int base;
    int i;

    exec_name = program;

    base = out_here();
    for (i = 0; i < n; i++)
        out_byte(stub[i]);

    /* The two routines that are written at the end, once there is an address
     * and a length for what they work on: clearing what starts at zero, and
     * making argc and argv out of what MOS passed. Both calls are here
     * whether they turn out to have anything to do or not -- four bytes and
     * a `ret` each, against working out how to not have made the call. */
    out_reloc(base + STUB_CLEAR_AT);
    bss_init_hole = base + STUB_CLEAR_AT;
    out_reloc(base + STUB_ARGS_AT);
    args_hole = base + STUB_ARGS_AT;

    /* Where exit unwinds to, and the stack it unwinds onto. Two cells
     * written by the stub just before it calls main: the stack pointer
     * then, and the address of the instruction that main returns to. exit
     * puts the second in the program counter with the first in the stack
     * pointer, and the tail below runs as though main had returned. See
     * gen_exit.
     *
     * They live here, in the image behind the stub, and not in the bss.
     * Six bytes of RAM either way, and here they cost nothing to clear:
     * the stub writes both before anything reads either, so zero is not a
     * value they ever have to start at. A program with nothing else at
     * zero then still has nothing there, and the routine that clears it is
     * still a bare `ret`.
     *
     * The stub writes them whether the program calls exit or not, which is
     * nineteen bytes a program. Working out whether it does would mean
     * knowing before the file is read. */
    for (i = 0; i < 2 * ACC_INT_SIZE; i++)
        out_byte(0);
    sym_at(exit_cell(0))->val = out_here() - 2 * ACC_INT_SIZE;
    sym_at(exit_cell(1))->val = out_here() - ACC_INT_SIZE;

    /* And the exit hook: what the stub calls on the way out, whether main
     * returned or exit sent it there -- see src/rt/startup.s. It starts at
     * the `ret` that ends the stub, and the library's atexit and fopen point
     * it at the routine that runs atexit's functions and closes the files.
     * A name, like the other two, so that the library can reach it. */
    out_reloc(out_here());
    out_word24(base + n - 1);
    sym_at(exit_cell(2))->val = out_here() - ACC_INT_SIZE;
    out_reloc(base + STUB_HOOK_AT);
    out_patch24(base + STUB_HOOK_AT, out_here() - ACC_INT_SIZE);
    out_reloc(base + STUB_HOOK_CALL_AT);
    out_patch24(base + STUB_HOOK_CALL_AT, base + n - 2);

    /* And a third, for MOS's stack pointer while main runs on its own at the
     * top of the program's memory -- which is where the heap already leaves
     * room for it. This one only the stub reads, so it needs no name. */
    {
        int mos_sp = out_here();

        for (i = 0; i < ACC_INT_SIZE; i++)
            out_byte(0);
        out_reloc(base + STUB_MOS_SP_AT);
        out_patch24(base + STUB_MOS_SP_AT, mos_sp);
        out_reloc(base + STUB_MOS_SP_BACK_AT);
        out_patch24(base + STUB_MOS_SP_BACK_AT, mos_sp);
        /* The top of memory moves with where the image is loaded, so it is
         * relocated -- but not by the pass that shortens jumps, which moves
         * every address in the image along with the code, and this one is
         * past the image: written straight away it came out 71 bytes low,
         * that being how far the jumps had shrunk. So it is a name the link
         * answers, after everything is laid down, as the heap's end is. */
        stack_top_at = base + STUB_TOP_AT;     /* see gen_finish */
    }
    sym_set_flags(exit_cell(0), SYMF_DEFINED);
    sym_set_flags(exit_cell(1), SYMF_DEFINED);
    sym_set_flags(exit_cell(2), SYMF_DEFINED);
    fixup_add(exit_cell(0), base + STUB_SP_AT);
    fixup_add(exit_cell(1), base + STUB_PCSTORE_AT);
    out_reloc(base + STUB_PC_AT);
    out_patch24(base + STUB_PC_AT, base + STUB_AFTER_MAIN);

    fixup_add(m, base + STUB_MAIN_AT);

    if (ending == END_PRINT)
        for (i = 0; i < (int) (sizeof print_calls / sizeof *print_calls); i++) {
            out_reloc(base + print_calls[i].at);
            out_patch24(base + print_calls[i].at, base + print_calls[i].to);
        }
}

int gen_local(int size)
{
    locals_size += size;

    return -locals_size;
}

/* How many bytes of declared locals are kept where (ix+d) can reach them.
 *
 * (ix+d) reaches 128 bytes below the frame pointer, and the scratch area
 * shares them with the locals -- which is what used to make a function with
 * a few dozen variables in it refuse to compile at all.
 *
 * The line is drawn from measurement, and drawn so that nothing that
 * compiles today gets slower. Over the 1012 functions in acc's own source,
 * its library and its test cases, the most any of them declares is 84 bytes
 * of locals, and the scratch -- now that its slots are reused -- wants four
 * on average. So the locals keep 96 of the 128 and the scratch has the rest:
 * every function in that corpus keeps every one of its locals in reach of
 * (ix+d), which is the fast way to touch one and the only reason to have a
 * frame pointer at all.
 *
 * What a function declares past 96 goes where the arrays and the structs
 * already go, and is reached by a computed address -- ten bytes and a couple
 * of dozen cycles on every use of it, against the alternative, which was to
 * refuse to compile the function at all. A function whose scratch then wants
 * more than the 32 left over is still refused, which is what it was before;
 * that is the remaining half of this, and it is the scratch's half. */
#define NEAR_LOCALS 96

int gen_local_fits(int size)
{
    return locals_size + size <= NEAR_LOCALS;
}

int gen_local_far(int size)
{
    int array = gen_local_array();

    gen_local_array_size(array, size);

    return array;
}

/* The stack, as an array whose length only the program knows.
 *
 * The frame itself is reached through ix and does not move, so taking room
 * off the stack costs nothing but sp: the bytes are below everything the
 * frame holds, and `ld sp, ix` in the epilogue gives them all back at once.
 * What these three add is giving them back earlier -- at the end of the
 * block that took them, so that a loop does not take the room again on
 * every turn. */
void gen_stack_take(int slot)
{
    save_regs_below(1);
    force_into(vsp - 1, R_HL);          /* the byte count */
    vdrop();
    evict_reg(R_DE);
    ex_de_hl();
    ld_rr_imm(R_HL, 0);
    out_byte(0x39);                     /* add hl, sp */
    or_a_a();
    sbc_hl_rr(R_DE);                    /* hl = sp - bytes */
    out_byte(0xf9);                     /* ld sp, hl */
    ld_ix_rr(slot, R_HL);               /* and that is where the array is */
}

void gen_stack_mark(int slot)
{
    evict_reg(R_HL);
    ld_rr_imm(R_HL, 0);
    out_byte(0x39);                     /* add hl, sp */
    ld_ix_rr(slot, R_HL);
}

void gen_stack_back(int slot)
{
    evict_reg(R_HL);
    ld_rr_ix(R_HL, slot);
    out_byte(0xf9);                     /* ld sp, hl */
}

int gen_local_array(void)
{
    if (narrays == array_end_cap) {
        array_end_cap = array_end_cap ? array_end_cap * 2 : 16;
        array_end = realloc(array_end, (size_t) array_end_cap * sizeof *array_end);
        if (!array_end)
            acc_error("out of memory for local arrays");
    }
    array_end[narrays] = arrays_size;

    return narrays++;
}

/* Called once per array, before the next one is declared. */
void gen_local_array_size(int array, int size)
{
    arrays_size += size;
    array_end[array] = arrays_size;
}

/* The address of a local array's first element, in HL:
 *
 *   push de / ld de, K / push ix / pop hl / add hl, de / pop de
 *
 * with K patched when the function ends. DE is kept, because whatever it
 * holds may still be wanted. */
void vaddr_array(int array, Type elem)
{
    if (type_ptr_depth(elem) == TY_PTR_MAX)
        acc_error_at(tok_line, "a pointer can be %d deep and this is deeper",
                     TY_PTR_MAX);

    evict_reg(R_HL);
    out_byte2(0xd5, 0x11);               /* push de; ld de, nn */
    if (narray_patches == array_patches_cap) {
        array_patches_cap = array_patches_cap ? array_patches_cap * 2 : 16;
        array_patches = realloc(array_patches,
                                (size_t) array_patches_cap * sizeof *array_patches);
        if (!array_patches)
            acc_error("out of memory for local arrays");
    }
    array_patches[narray_patches].at = out_here();
    array_patches[narray_patches].array = array;
    narray_patches++;
    out_word24(0);
    out_byte2(0xdd, 0xe5);               /* push ix */
    out_byte3(0xe1, 0x19, 0xd1);         /* pop hl; add hl, de; pop de */
    vpush(VAL_REG, type_ptr_to(elem), R_HL);
}

/* A string literal's bytes, and the terminator, somewhere in the image:
 * returns where. Its address is known the moment it is written, which is
 * what lets a string be an ordinary constant pointer.
 *
 * At file scope nothing runs between the declarations, so the bytes go where
 * the output is. Inside a function the code is running through here, so
 * they are jumped over: four bytes of code and a jump taken each time, which
 * is what writing them where they are read costs -- the alternative, a pool
 * after the code with every use patched, makes the address unknown until the
 * end and a string no longer a constant. */
int gen_data_bytes;

int gen_data(const char *bytes, int len)
{
    int over = in_function ? gen_jump() : -1, at, i;

    at = out_here();
    for (i = 0; i < len; i++)
        out_byte((unsigned char) bytes[i]);
    out_byte(0);
    gen_data_bytes += len + 1;
    if (over >= 0)
        gen_label(over);

    return at;
}

/* `count` bytes from address `from` to a local array at `offset`: a string
 * copied into the char array it initialises. Only at a declaration, where no
 * value is being held anywhere.
 *
 *   (the array's address in HL) / ex de, hl / ld hl, from / ld bc, count / ldir */
void gen_copy_to_array(int array, int offset, int from, int count)
{
    vaddr_array(array, TY_CHAR);
    vdrop();
    if (offset) {
        out_byte(0x11);                  /* ld de, offset */
        out_word24(offset);
        out_byte(0x19);                  /* add hl, de */
    }
    out_byte(0xeb);                      /* ex de, hl */
    out_byte(0x21);                      /* ld hl, from */
    out_reloc(out_here());
    out_word24(from);
    out_byte(0x01);                      /* ld bc, count */
    out_word24(count);
    out_byte2(0xed, 0xb0);               /* ldir */
}

/* Bytes [from, from + size) of a local array set to zero, which is what the
 * elements an initialiser does not mention start as: the first byte cleared,
 * and ldir copying it along the rest. Only at a declaration, where no value
 * is being held anywhere. */
void gen_zero_array(int array, int from, int size)
{
    vaddr_array(array, TY_CHAR);
    vdrop();
    if (from) {
        out_byte(0x11);                  /* ld de, from */
        out_word24(from);
        out_byte(0x19);                  /* add hl, de */
    }
    out_byte2(0x36, 0x00);               /* ld (hl), 0 */
    if (size > 1) {
        out_byte3(0xe5, 0xd1, 0x13);     /* push hl; pop de; inc de */
        out_byte(0x01);                  /* ld bc, nn */
        out_word24(size - 1);
        out_byte2(0xed, 0xb0);           /* ldir */
    }
}

/* Where the scratch area is free from: past the end of everything in it that
 * is still wanted.
 *
 * What a spilled register holds is on the value stack -- that is why it was
 * spilled -- so a slot the stack does not point at is nobody's. The slots
 * used to be handed out in order and all released together at the end of the
 * statement, which made the area as large as everything a statement ever
 * spilled rather than as large as it holds at its worst moment. One call
 * with six arguments and a `?:` in each wanted 55 bytes that way, a function
 * full of them 580, and (ix+d) reaches 128: that is what made a function
 * with a few dozen variables in it refuse to compile.
 *
 * spill_locked is for the one thing that lives in the scratch without the
 * stack saying so: the slot a `?:` parks its middle operand in has to
 * survive the compiling of the third operand, and nothing on the stack
 * points at it while that happens. */
/*
 * And the slots handed out that the stack does not point at yet. A slot is
 * taken before the value is built in it, and building it may need a
 * register that is holding something -- which is spilled, to a slot of its
 * own, while the stack still has no word of the first one. -inf < x as an
 * argument after another comparison spilled the first answer into the
 * float being negated. So such a slot stays reserved until vpush_scratch
 * puts it on the stack, or the statement ends. */
static int spill_free_from(void)
{
    int floor = spill_locked, i, size;

    for (i = 0; i < vtop; i++) {
        int start = spill_start_of(vstack + i, &size);

        if (start >= 0 && start + size > floor)
            floor = start + size;
    }
    for (i = 0; i < nspill_pending; i++)
        if (spill_pending[i].end > floor)
            floor = spill_pending[i].end;

    return floor;
}

static int spill_take(int size)
{
    int start = spill_free_from();

    spill_used = start + size;
    if (spill_used > spill_peak)
        spill_peak = spill_used;

    return -(locals_size + spill_used);
}

/* A slot for a value about to be built in it and then pushed. It is
 * reserved until it is pushed: see vpush_scratch. */
static int spill_slot_of(int size)
{
    int disp = spill_take(size);

    /* Full: the oldest gives way. It has not happened; four is two more
     * than any operator here holds at once. */
    if (nspill_pending == SPILL_PENDING) {
        memmove(spill_pending, spill_pending + 1,
                (SPILL_PENDING - 1) * sizeof *spill_pending);
        nspill_pending--;
    }
    spill_pending[nspill_pending].disp = disp;
    spill_pending[nspill_pending].end = spill_used;
    nspill_pending++;

    return disp;
}

/* A slot for a register being spilled, whose value is on the stack the
 * moment it is stored: nothing to reserve. */
static int spill_slot(void)
{
    return spill_take(ACC_INT_SIZE);
}

/* A scratch slot's value pushed, and so on the stack, which now says the
 * slot is taken: its reservation is done with. */
static void vpush_scratch(Type type, int slot)
{
    unsigned char i;

    vpush(VAL_LOCAL, type, slot);
    for (i = 0; i < nspill_pending; i++)
        if (spill_pending[i].disp == slot) {
            spill_pending[i] = spill_pending[--nspill_pending];

            return;
        }
}

/* What gen_stmt_end frees, but only as far as nothing still wants it: the
 * end of a value inside an expression rather than of a statement. An
 * initialiser's elements end this way, and a compound literal's initialiser
 * is inside whatever expression it appears in -- the third operand of a `?:`,
 * say, whose middle is parked in a slot nothing on the stack points at.
 * Freeing all of it there let the rest of that operand build over the slot,
 * and `n > 1 ? f8() : (int){ n } ? ...` answered with what it had built. */
void gen_value_end(void)
{
    const Value *v;

    spill_used = spill_free_from();
    nwide_consts = 0;
    for (v = vstack; v < vsp; v++)
        if (v->kind == VAL_WIDE && v->val >= nwide_consts)
            nwide_consts = v->val + 1;
}

void gen_func_begin(int fn, int nparams, Type returns)
{
    return_type = returns;
    return_ext = sym_at(fn)->ext;

    (void) nparams;

    sym_at(fn)->val = out_here();
    static_begin(fn);
    mark_here(&func_mark);
    vtop = 0;
    vsp = vstack;
    locals_size = 0;
    spill_used = 0;
    spill_peak = 0;
    spill_locked = 0;
    nspill_pending = 0;
    arrays_size = 0;
    narrays = 0;
    narray_patches = 0;
    in_function = 1;

    /* push ix / ld ix, 0 / add ix, sp -- the frame agondev's __frameset
     * builds, written out rather than called, because there is nothing to
     * link against yet. ix then points at the saved ix, so the first argument
     * is at ix+6: three bytes of saved ix and three of return address. */
    out_byte2(0xdd, 0xe5);              /* push ix */
    out_byte2(0xdd, 0x21);              /* ld ix, 0 */
    out_word24(0);
    out_byte2(0xdd, 0x39);              /* add ix, sp */

    /* ld hl, -frame / add hl, sp / ld sp, hl. The size is not known until the
     * body has been read, so the space is reserved and filled in at the end. */
    out_byte(0x21);                              /* ld hl, nn */
    frame_patch = out_here();
    out_word24(0);
    out_byte2(0x39, 0xf9);                       /* add hl, sp; ld sp, hl */                              /* ld sp, hl */
}

void gen_func_end(void)
{
    /* Restoring sp from ix unconditionally costs two bytes in a function with
     * no locals and saves the epilogue having to know the frame size. */
    out_byte2(0xdd, 0xf9);              /* ld sp, ix */
    out_byte2(0xdd, 0xe1);              /* pop ix */
    out_byte(0xc9);                              /* ret */

    out_patch24(frame_patch, -frame_size());
    in_function = 0;

    {
        int above = locals_size + spill_peak, i;

        for (i = 0; i < narray_patches; i++)
            out_patch24(array_patches[i].at,
                        -(above + array_end[array_patches[i].array]));
    }

    /* Last, so that everything written into the function is written before
     * any of it moves -- and before static_end measures how long it is. */
    relax_function(&func_mark);
    static_end();
}

void gen_return(int line)
{
    /* C99 has a return with a value only in a function that returns one, and
     * one without only in a function that does not. */
    if (return_type == TY_VOID && vtop > 0)
        acc_error_at(line, "this function returns void, so 'return' cannot "
                           "give it a value");
    if (return_type != TY_VOID && vtop == 0)
        acc_error_at(line, "this function returns a value, so 'return' needs "
                           "one");

    /* A struct is copied to where the caller asked for it, which is the
     * hidden first argument, and that address is the answer in HL -- as
     * agondev does it. */
    if (type_is_struct(return_type)) {
        Value *top = vsp - 1;

        if (!type_is_struct(top->type) || top->ext != return_ext)
            acc_error_at(line, "this function returns a struct, and 'return' "
                               "has to give it one of the same type");
        top->type = type_ptr_to(TY_CHAR);
        force_into(top, R_HL);
        ld_rr_ix(R_DE, 2 * ACC_PTR_SIZE);
        ld_rr_imm(R_BC, ext_bytes(return_ext));
        out_byte2(0xed, 0xb0);          /* ldir */
        ld_rr_ix(R_HL, 2 * ACC_PTR_SIZE);
        vdrop();
        out_byte2(0xdd, 0xf9);          /* ld sp, ix */
        out_byte2(0xdd, 0xe1);          /* pop ix */
        out_byte(0xc9);                 /* ret */

        return;
    }

    /* The result goes in HL, which is where agondev returns an int as well --
     * worth matching even with nothing to link against, because it is what
     * lets the two be mixed later. */
    if (vtop > 0) {
        int reg;

        if (type_wide(return_type)) {
            /* HL with the high byte in E, which is where agondev puts a
             * four-byte result. The value is in the frame, so this is two
             * loads. An eight-byte one is HL, DE and BC from the bottom up,
             * BC's upper byte reading one past the slot, which the caller
             * does not look at. */
            int at;

            vconvert(return_type);
            wide_needs_slot();
            at = (vsp - 1)->val;
            if (type_eight(return_type)) {
                ld_rr_ix(R_HL, at);
                ld_rr_ix(R_DE, at + ACC_INT_SIZE);
                ld_rr_ix(R_BC, at + 2 * ACC_INT_SIZE);
            } else {
                ld_rr_ix(R_HL, at);
                ld_e_ix(at + ACC_INT_SIZE);
            }
            vdrop();
            out_byte2(0xdd, 0xf9);      /* ld sp, ix */
            out_byte2(0xdd, 0xe1);      /* pop ix */
            out_byte(0xc9);                      /* ret */

            return;
        }

        vconvert(return_type);
        reg = vpop_reg();
        if (reg != R_HL)
            mov_rr(R_HL, reg);

        /* A one-byte result goes in A. HL keeps the widened value as well,
         * which costs one byte and is what lets a call to a function defined
         * further down the file -- where the return type is not known yet --
         * still read its answer. */
        if (RETURNS_IN_A(return_type))
            ld_a_l();
    }
    out_byte2(0xdd, 0xf9);              /* ld sp, ix */
    out_byte2(0xdd, 0xe1);              /* pop ix */
    out_byte(0xc9);                              /* ret */
}

/* That a struct argument and its parameter are the same struct: a struct
 * cannot be converted to anything, nor anything to one. */
__attribute__((noinline))
static void struct_argument(Type param, int param_ext, int which)
{
    if (!type_is_struct(param) || !type_is_struct(vtype())
        || (vsp - 1)->ext != param_ext)
        acc_error_at(tok_line, "argument %d has to be %s", which + 1,
                     type_is_struct(param) ? "a struct of the parameter's type"
                                           : "a number, not a struct");
}

/* A struct argument, from the address on top: its bytes copied onto the
 * stack, in as many whole slots as they fill, the struct at the lowest
 * address -- as agondev passes one. Returns the slots it took. */
__attribute__((noinline))
static int push_struct(void)
{
    int bytes = ext_bytes((vsp - 1)->ext);
    int slots = (bytes + ACC_INT_SIZE - 1) / ACC_INT_SIZE;

    /* ldir takes all three registers, and the arguments still to go may be
     * in them. */
    save_regs_below(1);
    (vsp - 1)->type = type_ptr_to(TY_CHAR);
    force_into(vsp - 1, R_HL);
    ex_de_hl();                             /* DE: the struct */
    ld_rr_imm(R_HL, -slots * ACC_INT_SIZE);
    out_byte2(0x39, 0xf9);                  /* add hl, sp; ld sp, hl */
    ex_de_hl();                             /* HL: the struct, DE: its copy */
    ld_rr_imm(R_BC, bytes);
    out_byte2(0xed, 0xb0);                  /* ldir */
    vdrop();

    return slots;
}

/* Arguments are pushed right to left, each in a whole three-byte slot, and
 * the caller takes them off again -- which is agondev's convention. */
/* What a call calls: a function by name, or whatever a pointer below the
 * arguments points at -- with the type it returns either way. */
typedef struct {
    Type type;
    int  ext, quals;
    int  fn;                    /* the function, or SYM_NONE for a pointer */
} Callee;

static void call_to(const Callee *callee, int nargs, int params_first,
                    int nparams);

/* exit: back to the stub that called main, as though main had returned.
 *
 * Not a library routine, because there is nothing in C to write it with: it
 * has to put the stack back where it was before main was entered and carry
 * on from where main would have returned to. The stub left both of those
 * in two cells behind itself -- see gen_startup -- so this is four
 * instructions with the answer already in HL, written out at the call
 * rather than called, which saves the return nobody comes back for.
 *
 * Whatever main's arguments left on the stack is still above the restored
 * pointer, and the stub's `pop bc` twice takes them off, exactly as if main
 * had returned in the ordinary way. */
static int exit_builtin(const Sym *f, int nargs)
{
    if (nargs != 1 || f->type != TY_VOID
        || strcmp(name_text(f->name), "exit") != 0)
        return 0;

    save_regs_below(nargs);
    force_into(vsp - 1, R_HL);          /* the status, which the tail reads */
    vdrop();

    out_byte(0xed);
    out_opcode24(0x5b, 0);              /* ld de, (carry on at) */
    fixup_add(exit_cell(1), out_here() - ACC_INT_SIZE);
    out_byte(0xed);
    out_opcode24(0x7b, 0);              /* ld sp, (the stack main was on) */
    fixup_add(exit_cell(0), out_here() - ACC_INT_SIZE);
    push_rr(R_DE);
    out_byte(0xc9);                     /* ret, into what DE was */

    vpush(VAL_VOID, TY_VOID, 0);

    return 1;
}

/* memcpy, memmove, memset and memchr, done by the instructions that do them.
 *
 * The eZ80 copies a block with ldir and fills one with ldir reading its own
 * output a byte behind; C says it a byte at a time, and acc compiled that
 * faithfully into a loop that cost zap seven percent of everything it ran.
 * So a call to one of the three by name goes to a helper written in the
 * instruction instead, with the operands in the registers it wants rather
 * than on the stack.
 *
 * By name, which C allows: these three are the implementation's to define,
 * and a program that writes its own means the library's, not something of
 * its own that happens to be spelled the same. The library still has all
 * three compiled from C, for a program that takes one's address.
 *
 * Returns 0 when this is not one of them, or not the shape of one, and the
 * ordinary call is emitted instead. */
static int mem_builtin(const Sym *f, int nargs)
{
    const char *name;
    int which;

    if (nargs != 3 || !type_pointer(f->type))
        return 0;
    name = name_text(f->name);
    if (strcmp(name, "memcpy") == 0)
        which = RT_MEMCPY;
    else if (strcmp(name, "memmove") == 0)
        which = RT_MEMMOVE;
    else if (strcmp(name, "memset") == 0)
        which = RT_MEMSET;
    else if (strcmp(name, "memchr") == 0)
        which = RT_MEMCHR;
    else
        return 0;

    /* Nothing below the arguments may be left in a register: ldir takes
     * three of them and the helper is free with the rest, exactly as a call
     * would be. */
    save_regs_below(nargs);

    /* How many, first: it is the last argument and the top of the stack, so
     * taking it now leaves the other two where force_into expects them. */
    force_into(vsp - 1, R_BC);

    if (which == RT_MEMSET || which == RT_MEMCHR) {
        force_into(vsp - 2, R_HL);      /* the byte, which goes in A */
        ld_a_l();
        force_into(vsp - 3, R_HL);      /* and where to put it or find it */
    } else {
        force_into(vsp - 2, R_HL);      /* the source, which ldir reads */
        force_into(vsp - 3, R_DE);      /* and the destination it writes */
    }

    rt_call(which);
    vdrop();
    vdrop();
    vdrop();
    vpush_reg(R_HL);
    (vsp - 1)->type = f->type;
    (vsp - 1)->ext = (unsigned char) f->ext;

    return 1;
}

void gen_call(int fn, int nargs, int params_first, int nparams)
{
    gen_effects++;
    Callee callee;
    const Sym *f = sym_at(fn);

    if (mem_builtin(f, nargs) || exit_builtin(f, nargs))
        return;

    callee.type = f->type;
    callee.ext = f->ext;
    callee.quals = f->quals & SQ_CONST ? VQ_CONST : 0;
    callee.fn = fn;
    call_to(&callee, nargs, params_first, nparams);
}

/* A function's address, as a pointer to it: a constant when it is already
 * defined, and otherwise loaded with a hole that gen_finish fills in, as a
 * call to it would be. */
int gen_data_context;
int gen_pending_sym = SYM_NONE;

/* A variable whose address is not known yet, which is what a declaration
 * that only says extern leaves behind: some file defines it, and which
 * address that is comes from the linker -- or from gen_finish, when this
 * file turns out to define it further down.
 *
 * The same shape as a call to a function not yet seen: a load of zero with
 * the slot written down. It costs a register load where a variable with an
 * address is a constant the value stack can carry about and fold into an
 * index, which is why it is only done for the ones that need it. */
void vpush_global_addr(int sym)
{
    /* In a global's initial value there is no code to put a hole in, so the
     * bytes are the hole: the same path a function's address takes. */
    if (gen_data_context) {
        if (gen_pending_sym != SYM_NONE)
            acc_error_at(tok_line, "one address not yet known in each initial "
                                   "value, at most");
        gen_pending_sym = sym;
        vpush_const(0, type_ptr_to(sym_at(sym)->type));

        return;
    }
    vpush_const(0, type_ptr_to(sym_at(sym)->type));
    force_reg(vsp - 1);
    fixup_add(sym, out_here() - ACC_INT_SIZE);
    fixups[nfixups - 1].declared = 1;   /* an address: the type is no matter */
}

void vpush_function(int fn)
{
    const Sym *f = sym_at(fn);

    want(fn);

    /* Asked of the flag rather than of the address, here and at the call
     * below, because an object puts its first function at offset zero --
     * and zero is also what a function that has not been defined has. Taking
     * the second path for a function that is in fact defined would still
     * come out right, since the fixup is filled in with the same address,
     * but not with the same instructions: this one leaves a constant on the
     * value stack for whatever wants it, and that one forces a register. */
    if (sym_flags(fn) & SYMF_DEFINED) {
        vpush_const(f->val, type_ptr_to(TY_FUNC));
        vset_addr();

        return;
    }

    /* In a global's initial value there is no code to put the hole in: the
     * value is 0 for now, and the caller fills its bytes in once it knows
     * where they went -- see gen_data_fixup. */
    if (gen_data_context) {
        if (gen_pending_sym != SYM_NONE)
            acc_error_at(tok_line, "one function not yet defined in each "
                                   "initial value, at most");
        gen_pending_sym = fn;
        vpush_const(0, type_ptr_to(TY_FUNC));

        return;
    }
    vpush_const(0, type_ptr_to(TY_FUNC));
    force_reg(vsp - 1);
    fixup_add(fn, out_here() - ACC_INT_SIZE);
    fixups[nfixups - 1].declared = 1;   /* an address: the type is no matter */
}

/* The three bytes at `at`, in the image, are a function's address, which
 * gen_finish writes there once the function is defined. */
void gen_data_fixup(int fn, int at)
{
    fixup_add(fn, at);
    fixups[nfixups - 1].declared = 1;
}

/* A call through the pointer to a function under the arguments. */
void gen_call_indirect(int nargs)
{
    gen_effects++;
    Value *fp = vsp - 1 - nargs;
    Callee callee;
    int x = fp->ext;

    if (fp->type != type_ptr_to(TY_FUNC))
        acc_error_at(tok_line, "only a function or a pointer to one can be "
                               "called");
    callee.type = ext_elem(x);
    callee.ext = ext_elem_x(x);
    callee.quals = 0;
    callee.fn = SYM_NONE;
    call_to(&callee, nargs, ext_func_first(x),
            ext_func_declared(x) ? ext_func_count(x) : 0);
}

/* Names an object wants in the program without holding an address of
 * them. There is one: printf reaches its floating conversions through a
 * weak reference, so that a program that only prints integers does not
 * carry them, and a file that passes a float or a double to a function
 * that takes more than it names asks for them here. The link takes them
 * from the library if it has them, and says nothing if it has not -- a
 * variadic function of the program's own is not printf. */
static const char *wants_named[4];
static int         nwants_named;

static void gen_want(const char *name)
{
    int i;

    for (i = 0; i < nwants_named; i++)
        if (!strcmp(wants_named[i], name))
            return;
    if (nwants_named < (int) (sizeof wants_named / sizeof *wants_named))
        wants_named[nwants_named++] = name;
}

int gen_nwants(void)
{
    return nwants_named;
}

const char *gen_want_name(int i)
{
    return wants_named[i];
}

static void call_to(const Callee *callee, int nargs, int params_first,
                    int nparams)
{
    int i, argslots = 0;

    /* Anything still live in a register has to come out before the call.
     * The result comes back in HL and the callee is free with the rest, so a
     * value left in one does not survive -- which is how `f(..) - g(..)` lost
     * f's answer the moment g was called. The arguments are exempt: they are
     * about to be pushed and consumed. */
    save_regs_below(nargs);

    for (i = 0; i < nargs; i++) {
        /* Converted to the type the parameter was declared with. The
         * arguments come off the stack last one first, so this is the
         * parameter that many from the end. */
        int which = nargs - 1 - i;

        /* A struct argument has to meet a struct parameter; anything else
         * meeting one is refused by the conversion. */
        if (type_is_struct((vsp - 1)->type)) {
            if (which < nparams)
                struct_argument(sym_param_type(params_first, which),
                                sym_param_ext(params_first, which), which);
            argslots += push_struct();
            continue;
        }
        if (which >= nparams && gen_objects && type_float((vsp - 1)->type))
            gen_want("acc_format_float");
        if (which < nparams) {
            Type param = sym_param_type(params_first, which);

            if (type_is_struct(param))
                struct_argument(param, sym_param_ext(params_first, which),
                                which);
            vconvert(param);
        }

        if (type_eight(vtype())) {
            /* Three slots, nine bytes, which is what agondev gives a long
             * long: the top one first, so that the eight bytes lie in order
             * from the lowest address. The ninth is read from past the end
             * of the value and means nothing. */
            int slot;

            /* The pushes below go through HL, and an argument still to be
             * pushed may be sitting in it -- `f(&a, 1L)` lost the address.
             * Everything but this argument goes to the frame first. */
            save_regs_below(1);
            wide_needs_slot();
            slot = (vsp - 1)->val;

            ld_rr_ix(R_HL, slot + 2 * ACC_INT_SIZE);
            push_rr(R_HL);
            ld_rr_ix(R_HL, slot + ACC_INT_SIZE);
            push_rr(R_HL);
            ld_rr_ix(R_HL, slot);
            push_rr(R_HL);
            vdrop();
            argslots += 3;
        } else if (type_wide(vtype())) {
            /* Two slots, six bytes, which is what agondev gives a long. The
             * high half goes first because the stack grows downwards, so the
             * low bytes end up at the lower address. HL is used, so the
             * arguments still to go come out of the registers first. */
            int slot;

            save_regs_below(1);
            wide_needs_slot();
            slot = (vsp - 1)->val;

            ld_rr_imm(R_HL, 0);
            ld_l_ix(slot + ACC_INT_SIZE);       /* not through E: DE may hold
                                                 * an argument still to go */
            push_rr(R_HL);
            ld_rr_ix(R_HL, slot);
            push_rr(R_HL);
            vdrop();
            argslots += 2;      /* six bytes, so two slots to take back */
        } else {
            int reg = vpop_reg();

            push_rr(reg);
            argslots++;
        }
    }

    /* A struct comes back in a temporary the caller provides, in the array
     * area, whose address is passed ahead of the arguments. */
    if (type_is_struct(callee->type)) {
        int temp = gen_local_array(), x = callee->ext;

        gen_local_array_size(temp, ext_bytes(x));
        vaddr_array(temp, TY_CHAR);
        push_rr(vpop_reg());
        argslots++;
    }

    if (callee->fn == SYM_NONE) {
        call_through();
    } else if (sym_flags(callee->fn) & SYMF_DEFINED) {
        want(callee->fn);
        out_reloc(out_here() + 1);
        out_opcode24(0xcd, sym_at(callee->fn)->val);    /* call nn */
    } else {
        /* Defined further down the file, or not at all. The site is recorded
         * and filled in once the whole file has been read; gen_finish says so
         * if it never was. */
        out_opcode24(0xcd, 0);
        fixup_add(callee->fn, out_here() - ACC_INT_SIZE);
    }

    /* Discarded into DE, the cheapest form -- but for a long, whose high
     * byte comes back in E, into BC: popping DE put an argument's byte in
     * its place. */
    if (type_eight(callee->type)) {
        /* HL, DE and BC all hold the answer, so the arguments come off
         * into IY. */
        int slot = spill_slot_of(8);

        for (i = 0; i < argslots; i++)
            out_byte2(0xfd, 0xe1);              /* pop iy */
        ld_ix_rr(slot, R_HL);
        ld_ix_rr(slot + ACC_INT_SIZE, R_DE);
        out_byte(0x79);                         /* ld a, c */
        ld_ix_a(slot + 2 * ACC_INT_SIZE);
        out_byte(0x78);                         /* ld a, b */
        ld_ix_a(slot + 7);
        vpush_scratch(callee->type, slot);
        (vsp - 1)->ext = (unsigned char) callee->ext;

        return;
    }

    for (i = 0; i < argslots; i++)
        pop_rr(type_wide(callee->type) ? R_BC : R_DE);

    if (type_wide(callee->type)) {
        /* HL with the high byte in E; put it where every long lives. */
        int slot = spill_slot_of(ACC_LONG_SIZE);

        ld_ix_rr(slot, R_HL);
        out_byte(0x7b);                          /* ld a, e */
        ld_ix_a(slot + ACC_INT_SIZE);
        vpush_scratch(callee->type, slot);
        (vsp - 1)->ext = (unsigned char) callee->ext;

        return;
    }

    /* The temporary's address, which the callee hands back in HL: the
     * struct, as a value. */
    if (type_is_struct(callee->type)) {
        vpush(VAL_REG, TY_STRUCT, R_HL);
        (vsp - 1)->ext = callee->ext;

        return;
    }

    /* A void function gives back nothing, and a value of kind VAL_VOID says
     * so: dropping it, as a statement does, is all it is good for. */
    if (callee->type == TY_VOID) {
        vpush(VAL_VOID, TY_VOID, 0);

        return;
    }

    /* Read the answer from where the callee's type says it is. */
    if (RETURNS_IN_A(callee->type)) {
        Type returns = callee->type;

        if (type_unsigned(returns))
            fill_hl_with_zero();
        else
            fill_hl_with_sign_of_a();
        ld_l_a();
    }
    vpush_reg(R_HL);
    (vsp - 1)->type = type_promote(callee->type);
    (vsp - 1)->ext = (unsigned char) callee->ext;
    (vsp - 1)->quals = (unsigned char) callee->quals;
}

/* The call itself, through the pointer now on top of the stack -- the
 * arguments are pushed. There is no `call (iy)`: the return address is
 * pushed by hand and the jump taken, which is what agondev's __indcallhl
 * does in a routine of its own. The pointer has been put in the frame or
 * is a constant, since everything below the arguments was saved from the
 * registers the arguments needed. */
static void call_through(void)
{
    Value *fp = vsp - 1;

    if (val_const(fp->kind)) {
        out_byte2(0xfd, 0x21);                  /* ld iy, nn */
        if (fp->kind == VAL_ADDR)
            out_reloc(out_here());
        else if (fp->kind == VAL_BSS)
            gen_bss_fixup(out_here());
        out_word24(fp->val);
    } else if (fp->kind == VAL_LOCAL && disp_fits(fp->val)) {
        out_byte3(0xdd, 0x31, fp->val);         /* ld iy, (ix+d) */
    } else {
        int reg = force_reg(fp);

        push_rr(reg);
        out_byte2(0xfd, 0xe1);                  /* pop iy */
    }
    vdrop();
    out_reloc(out_here() + 1);
    out_opcode24(0x21, out_here() + 7);         /* ld hl, back */
    push_rr(R_HL);
    out_byte2(0xfd, 0xe9);                      /* jp (iy) */
}

/* What an operator means when a pointer is one of its operands.
 *
 * `p + 1` is the next object, not the next byte, so the integer side is
 * multiplied by the width of what the pointer points at -- and `q - p` is
 * the other way about, a difference in bytes divided back down into a count
 * of objects. A pointer to a one-byte type needs neither, which is most of
 * why char is the type people reach for when they want the bytes.
 */
/* Whether two pointers may be compared, which is the one thing about the
 * pointer cases that vapply can finish for itself: an address compares as the
 * unsigned int it is. Kept out of vbinop_pointer so that vcmp still has the
 * single caller that lets it stay inlined there -- giving it a second one
 * turned every comparison in every program into a call. */
static void vcmp_pointer_check(Type left, Type right)
{
    if (!type_pointer(left) || !type_pointer(right) || left == right)
        return;

    /* A `void *` compares with a pointer to anything, which is what C says
     * and what makes `p == NULL` work when NULL is `(void *) 0` -- which is
     * how <stddef.h> spells it, and how every library that has ever been
     * written spells it. */
    if (type_deref(left) == TY_VOID || type_deref(right) == TY_VOID)
        return;

    acc_error_at(tok_line, "these are pointers to different types");
}

/* Whether two extensions that are not the same one are rows of the same
 * shape all the same: every VLA is its own extension, and C99 6.7.5.2p6
 * makes two arrays of the same element type compatible whatever their
 * lengths, a VLA's included. */
__attribute__((noinline))
static int same_rows(int a, int b)
{
    return (ext_vla_size(a) || ext_vla_size(b))
           && ext_elem(a) == ext_elem(b) && ext_elem_x(a) == ext_elem_x(b);
}

/* A step that is a row whose length the program works out, `int m[n][n]`'s:
 * what ext_bytes holds for one is the frame slot its size is in, which is
 * below the frame pointer and so negative -- past any size an array can
 * have. The constant just pushed for it becomes a read of that slot, which
 * is one compare and a store where it is written: a call, or a branch
 * round a path of its own, moved clang's code for vapply about enough to
 * cost 0.7% on matrix.c. */
#define vla_step_fix(step) \
    ((unsigned) (step) > 0x7fffffu ? (void) ((vsp - 1)->kind = VAL_LOCAL) \
                                   : (void) 0)

static void vbinop_pointer(int op, Type left, Type right)
{
    int both = type_pointer(left) && type_pointer(right);
    Type ptr = type_pointer(left) ? left : right;
    int ext = type_pointer(left) ? (vsp - 2)->ext : (vsp - 1)->ext;
    int quals = type_pointer(left) ? (vsp - 2)->quals : (vsp - 1)->quals;
    int step;

    if (type_deref(ptr) == TY_VOID)
        acc_error_at(tok_line, "a 'void *' does not say what it points at, so "
                               "arithmetic on it has no step to take");
    if (type_is_func(type_deref(ptr)))
        acc_error_at(tok_line, "a pointer to a function has no step to take");

    step = type_step(ptr, ext);
    if (step == 0 && type_is_array(type_deref(ptr)))
        acc_error_at(tok_line, "an array of unknown size has no step to take");

    if (both) {
        if (op != TK_MINUS)
            acc_error_at(tok_line, "%s does not take two pointers",
                         tok_spelling(op));
        if (left != right || ((vsp - 2)->ext != (vsp - 1)->ext
                              && !same_rows((vsp - 2)->ext, (vsp - 1)->ext)))
            acc_error_at(tok_line, "these are pointers to different types");

        /* Signed before the divide and not after it. A pointer counts as
         * unsigned -- type_unsigned takes the pointer bits as well as the
         * unsigned one -- so the difference came out of the subtraction
         * labelled unsigned, and dividing it by the width of the element
         * was an unsigned divide. That is the right answer whenever the
         * left pointer is the further along, and 1290554 rather than -1
         * when it is not. */
        vbinop(TK_MINUS);
        (vsp - 1)->type = TY_INT;
        if ((unsigned) step > 1u) {
            vpush_const(step, TY_INT);
            vla_step_fix(step);
            vbinop(TK_SLASH);
            (vsp - 1)->type = TY_INT;
        }

        return;
    }

    if (op != TK_PLUS && op != TK_MINUS)
        acc_error_at(tok_line, "%s does not take a pointer", tok_spelling(op));

    /* `n + p` is `p + n`; subtraction has no such freedom, and a pointer on
     * the right of one is the program's mistake. */
    if (!type_pointer(left)) {
        if (op == TK_MINUS)
            acc_error_at(tok_line, "'-' takes a pointer on its left, not its "
                                   "right");
        vswap();
    }

    if ((unsigned) step > 1u) {
        vpush_const(step, TY_INT);
        vla_step_fix(step);
        vbinop(TK_STAR);
    }
    vbinop(op);
    (vsp - 1)->type = ptr;
    (vsp - 1)->ext = (unsigned char) ext;
    (vsp - 1)->quals = (unsigned char) quals;
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
static Type common_wide(Type left, Type right)
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
void vapply(int op, Type narrow)
{
    Type left, right;

    if ((unsigned) vtop < 2)
        acc_error("internal: an operator with nothing to apply it to");

    left  = (vsp - 2)->type;
    right = (vsp - 1)->type;

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

/* ------------------------------------------------------------------ */
/* pointers                                                            */

/* A pointer on this machine is the three bytes an int is, so it lives in a
 * register like one and needs no widening anywhere. What it does need is a
 * width to read and write through it, and that is the type it points at --
 * which is the whole of what the pointer depth in the type byte is for.
 */

/* &local: the address of a frame slot, which lea computes without loading
 * the value first or touching the flags. */
void vaddr_local(int offset, Type type)
{
    int reg;

    if (type_ptr_depth(type) == TY_PTR_MAX)
        acc_error_at(tok_line, "a pointer can be %d deep and this is deeper",
                     TY_PTR_MAX);

    reg = reg_alloc();
    lea_rr_ix(reg, offset);
    vpush(VAL_REG, type_ptr_to(type), reg);
}

/* *p: the top is a pointer and becomes what it points at.
 *
 * Four bytes go to a frame slot, because that is where every four-byte value
 * lives; the rest come back in HL at int width, the narrow ones widened on
 * the way exactly as a load from a frame slot widens them. */
void vderef(void)
{
    Value *top = vsp - 1;
    Type to;
    int deref_ext = 0, deref_quals = 0;

    if (vtop == 0)
        acc_error("internal: nothing to dereference");
    if (top->bits) {
        bitfield_read();

        return;
    }

    /* `*fp` is the function fp points at, which is its address again. */
    if (top->type == type_ptr_to(TY_FUNC))
        return;
    if (!type_pointer(top->type))
        acc_error_at(tok_line, "'*' takes a pointer, and this is %s",
                     type_float(top->type) ? "a floating-point value"
                                           : "an integer");

    to = type_deref(top->type);
    if (to == TY_VOID)
        acc_error_at(tok_line, "a 'void *' does not say what it points at, so "
                               "it cannot be read through");

    /* What a pointer to an array points at is an array, and an array is the
     * address of its first element: the same address, as a pointer to the
     * element. Nothing is read. */
    if (type_is_array(to)) {
        int x = top->ext;
        Type elem = ext_elem(x);

        if (type_ptr_depth(elem) == TY_PTR_MAX)
            acc_error_at(tok_line, "a pointer can be %d deep and this is deeper",
                         TY_PTR_MAX);
        top->type = type_ptr_to(elem);
        top->ext = (unsigned char) ext_elem_x(x);

        return;
    }

    /* A struct is not read either: its value is its address, held as a
     * register or a slot so that the one thing it can never be is a
     * constant -- which keeps constant folding from mistaking it for a
     * number. */
    if (type_is_struct(to)) {
        if (val_const(top->kind))
            force_reg(top);
        top->type = TY_STRUCT;

        return;
    }

    /* A pointer read through a pointer keeps its extension: the chain is
     * one level shorter, and its bottom is the same. */
    if (type_pointer(to)) {
        deref_ext = top->ext;
        deref_quals = top->quals;
    }

    force_into(top, R_HL);

    if (type_wide(to)) {
        int n = type_wide_bytes(to);
        int slot = long_scratch(to);
        int i;

        for (i = 0; i < n; i++) {
            ld_a_hl();
            ld_ix_a(slot + i);
            if (i < n - 1)
                inc_hl();
        }
        vdrop();
        vpush_scratch(to, slot);

        return;
    }

    if (type_size(to) == ACC_INT_SIZE) {
        ld_hl_ind_hl();
    } else if (type_size(to) == 1) {
        ld_a_hl();
        if (type_unsigned(to))
            fill_hl_with_zero();
        else
            fill_hl_with_sign_of_a();
        ld_l_a();
    } else {
        /* Two bytes, read through IY: loading either into H or into L would
         * overwrite the pointer before the other had been read, and keeping
         * the low one in E, as this once did, overwrote whatever the
         * allocator was holding in DE. IY is the backend's own scratch. */
        out_byte3(0xe5, 0xfd, 0xe1);    /* push hl; pop iy */
        out_byte3(0xfd, 0x7e, 0x01);    /* ld a, (iy+1) */
        if (type_unsigned(to))
            fill_hl_with_zero();
        else
            fill_hl_with_sign_of_a();
        ld_h_a();
        out_byte3(0xfd, 0x6e, 0x00);    /* ld l, (iy+0) */
    }

    vdrop();
    vpush_reg(R_HL);
    (vsp - 1)->type = type_promote(to);
    if (type_pointer(to)) {
        (vsp - 1)->ext = (unsigned char) deref_ext;
        (vsp - 1)->quals = (unsigned char) deref_quals;
    }
}

/* A member's address from its struct's: the offset added, as bytes, and the
 * member's type put on it. A member that is itself an array or a struct is
 * an object like any other, reached the same way. */
void vmember(int offset, Type type, int ext, int quals)
{
    quals |= (vsp - 1)->quals;          /* a const struct's members are */
    if (type_ptr_depth(type) == TY_PTR_MAX)
        acc_error_at(tok_line, "a pointer can be %d deep and this is deeper",
                     TY_PTR_MAX);
    if (offset) {
        (vsp - 1)->type = type_ptr_to(TY_CHAR);
        vpush_const(offset, TY_INT);
        vapply(TK_PLUS, 0);
    }
    (vsp - 1)->type = type_ptr_to(type);
    (vsp - 1)->ext = (unsigned char) ext;
    (vsp - 1)->quals = (unsigned char) quals;
}

/* `a = b` for structs: the bytes copied with ldir, from b's address in HL to
 * a's in DE, and a's address -- the struct, as a value -- left as the
 * answer. */
static void vcopy_struct(void)
{
    Value *dest = vsp - 2, *src = vsp - 1;
    int x = dest->ext, bytes;

    if (!type_is_struct(src->type) || src->ext != x)
        acc_error_at(tok_line, "a struct can only be assigned a struct of the "
                               "same type");
    bytes = ext_bytes(x);

    dest->type = src->type = type_ptr_to(TY_CHAR);
    force_into(src, R_HL);
    force_into(dest, R_DE);
    evict_reg(R_BC);
    if (bytes) {
        push_rr(R_DE);
        ld_rr_imm(R_BC, bytes);
        out_byte2(0xed, 0xb0);              /* ldir */
        pop_rr(R_HL);
    } else {
        ex_de_hl();
    }
    vdrop();
    vdrop();
    vpush(VAL_REG, TY_STRUCT, R_HL);
    (vsp - 1)->ext = (unsigned char) x;
}

/* *p = v, with the pointer under the value on the stack. The value is left
 * behind, because an assignment is an expression and what it comes to is
 * what was assigned. */
void vstore_indirect(void)
{
    gen_effects++;
    Type to;
    int addr;

    if ((unsigned) vtop < 2)
        acc_error("internal: a store through nothing");
    if (!type_pointer((vsp - 2)->type))
        acc_error_at(tok_line, "'*' takes a pointer");

    to = type_deref((vsp - 2)->type);
    if (to == TY_VOID)
        acc_error_at(tok_line, "a 'void *' does not say what it points at, so "
                               "it cannot be written through");
    if (type_is_array(to))
        acc_error_at(tok_line, "an array cannot be assigned to as a whole");

    /* The object at the bottom of the chain is const -- what a pointer to
     * const points at, a const array's element, a const struct's member --
     * and this store is to it rather than to a pointer on the way. */
    if (((vsp - 2)->quals & VQ_CONST) && type_ptr_depth((vsp - 2)->type) == 1)
        acc_error_at(tok_line, "this is const, so it cannot be changed");
    if ((vsp - 2)->bits) {
        bitfield_write();

        return;
    }
    if (type_is_struct(to)) {
        vcopy_struct();

        return;
    }

    vconvert(to);

    if (type_wide(to)) {
        int n = type_wide_bytes(to);
        int slot = long_scratch(to);
        int i;

        /* The value first, because building it may want HL, and the address
         * afterwards, which is what is left underneath it. */
        materialise_long(slot, to);
        vdrop();
        force_into(vsp - 1, R_HL);
        for (i = 0; i < n; i++) {
            ld_a_ix(slot + i);
            ld_hl_a();
            if (i < n - 1)
                inc_hl();
        }
        vdrop();
        vpush_scratch(to, slot);

        return;
    }

    /* The value in HL and the address in DE, which is the way round that
     * makes a three-byte store one instruction between two exchanges. */
    force_into(vsp - 1, R_HL);
    addr = force_reg(vsp - 2);
    if (addr != R_DE) {
        evict_reg(R_DE);
        mov_rr(R_DE, addr);
        (vsp - 2)->val = R_DE;
    }

    if (type_size(to) == ACC_INT_SIZE) {
        ex_de_hl();
        ld_ind_hl_de();
        ex_de_hl();
    } else if (type_size(to) == 1) {
        ld_a_l();
        ld_de_a();
    } else {
        ld_a_l();
        ld_de_a();
        inc_de();
        ld_a_h();
        ld_de_a();
    }

    /* The value is the answer; the address has done its work. */
    {
        Value kept = *(vsp - 1);

        vdrop();
        vdrop();
        vpush(kept.kind, kept.type, kept.val);
        (vsp - 1)->ext = kept.ext;
        (vsp - 1)->quals = kept.quals;
    }
}

/* A copy of the value `depth` below the top, pushed: vdup, from further
 * down. A register is moved to the frame first, for vdup's reason. */
static void vpick(int depth)
{
    Value *from = vsp - 1 - depth;

    if (from->kind == VAL_ACC || from->kind == VAL_REG) {
        int off;

        /* The register first and the slot after: making the register may
         * spill another, to a slot the stack does not yet know is taken. */
        if (from->kind == VAL_ACC)
            force_reg(from);
        off = spill_slot();
        ld_ix_rr(off, from->val);
        from->kind = VAL_LOCAL;
        from->val = off;
    }
    vpush(from->kind, from->type, from->val);
    (vsp - 1)->ext = from->ext;
}

/* The unit a bit-field is read from and written to: its bytes, as the
 * unsigned type that many bytes are.
 *
 * Five, six and seven bytes are no type, so a field that spans them is
 * reached as eight -- which is one to three bytes past what the field
 * occupies, and can be one to three bytes past the object it is in. Reading
 * them is harmless here: there is no memory that faults on being read. They
 * are then written back with the value they were read with, the mask below
 * having kept every bit that is not the field's, so nothing beyond it
 * changes. What the mask must not be taken from is the field's own byte
 * count, which is why the unit's width is asked for separately. */
static Type bitfield_unit(const BitField *bf)
{
    static const Type units[9] = { 0, TY_UCHAR, TY_USHORT, TY_UINT, TY_ULONG,
                                   TY_ULLONG, TY_ULLONG, TY_ULLONG,
                                   TY_ULLONG };

    return units[bf->bytes];
}

/* A bit-field's mask, or the weight of its sign bit, as a constant of the
 * type it is about to be applied to. */
static void bitfield_const(uint64_t bits, Type type)
{
    if (type_eight(type))
        vpush_const_wide((uint32_t) bits, (uint32_t) (bits >> 32), type);
    else if (type_wide(type))
        vpush_const_long((long) (uint32_t) bits, type);
    else
        vpush_const((int) bits, type);
}

/* A bit-field's value, from its address on the stack: its unit read, shifted
 * down and masked -- and, for a signed one, the top bit it has spread up
 * through the rest. Narrower than an int, it is an int, as C promotes it. */
static void bitfield_read(void)
{
    const BitField *bf = bitfield_at((vsp - 1)->bits);
    Type unit = bitfield_unit(bf);
    int unit_bits = type_scalar_bytes(unit) * 8;
    uint64_t mask = bf->width >= 64 ? ~(uint64_t) 0
                                    : ((uint64_t) 1 << bf->width) - 1;

    (vsp - 1)->bits = 0;
    (vsp - 1)->type = type_ptr_to(unit);
    vderef();
    if (bf->pos) {
        vpush_const(bf->pos, TY_INT);
        vapply(TK_SHR, 0);
    }
    if (bf->pos + bf->width < unit_bits) {
        bitfield_const(mask, type_wide(unit) ? unit : TY_UINT);
        vapply(TK_AMP, 0);
    }
    if (type_wide(unit) && bf->width <= ACC_INT_SIZE * 8)
        vconvert(TY_UINT);
    if (bf->is_signed
        && bf->width < (type_wide(unit) ? unit_bits : ACC_INT_SIZE * 8)) {
        /* (v ^ sign) - sign: the sign bit's weight made negative. */
        Type t = type_eight(vtype()) ? TY_LLONG
                 : type_wide(vtype()) ? TY_LONG : TY_INT;
        uint64_t sign = (uint64_t) 1 << (bf->width - 1);

        bitfield_const(sign, t);
        vapply(TK_CARET, 0);
        bitfield_const(sign, t);
        vapply(TK_MINUS, 0);
    }
    if (!type_wide(vtype()))
        (vsp - 1)->type = bf->width < ACC_INT_SIZE * 8 || bf->is_signed
                          ? TY_INT : TY_UINT;
}

/* `f = v` for a bit-field f, with its address under v: its unit read, its
 * bits cleared and v's put there, and the unit written back. The answer is
 * v as the bit-field holds it -- cut to its width, and read back signed if
 * it is. */
static void bitfield_write(void)
{
    const BitField *bf = bitfield_at((vsp - 2)->bits);
    Type unit = bitfield_unit(bf);
    int unit_bits = type_scalar_bytes(unit) * 8;
    Type wide = type_wide(unit) ? unit : TY_UINT;
    uint64_t mask = bf->width >= 64 ? ~(uint64_t) 0
                                    : ((uint64_t) 1 << bf->width) - 1;
    uint64_t keep = ~(mask << bf->pos)
                    & (unit_bits >= 64 ? ~(uint64_t) 0
                                       : ((uint64_t) 1 << unit_bits) - 1);

    /* The value, cut to the width: [addr, v]. A _Bool field is 0 or 1
     * first, as any _Bool is. */
    if (type_deref((vsp - 2)->type) == TY_BOOL)
        vconvert(TY_BOOL);
    vconvert(wide);
    bitfield_const(mask, wide);
    vapply(TK_AMP, 0);

    /* Two copies of the address, to read the unit through and to write it
     * back through, and the unit read with the field's bits cleared:
     * [addr, v, addr, old]. */
    vpick(1);
    (vsp - 1)->bits = 0;
    (vsp - 1)->type = type_ptr_to(unit);
    vdup();
    vderef();
    bitfield_const(keep, wide);
    vapply(TK_AMP, 0);

    /* v moved into place and put in, [addr, v, addr, new], and written
     * back: [addr, v]. */
    vpick(2);
    if (bf->pos) {
        vpush_const(bf->pos, TY_INT);
        vapply(TK_SHL, 0);
    }
    vapply(TK_PIPE, 0);
    vstore_indirect();
    vdrop();

    /* The answer, v as the field reads: signed, when it is. [v]. */
    vswap();
    vdrop();
    if (bf->is_signed
        && bf->width < (type_wide(unit) ? unit_bits : ACC_INT_SIZE * 8)) {
        uint64_t sign = (uint64_t) 1 << (bf->width - 1);
        Type t = type_eight(unit) ? TY_LLONG
                 : type_wide(unit) ? TY_LONG : TY_INT;

        bitfield_const(sign, t);
        vapply(TK_CARET, 0);
        bitfield_const(sign, t);
        vapply(TK_MINUS, 0);
    }
    if (type_wide(unit) && bf->width <= ACC_INT_SIZE * 8)
        vconvert(bf->is_signed ? TY_INT : TY_UINT);
}

/* The top twice. A value in a register goes to the frame first: two
 * descriptors naming one register would be two owners of it, and the first
 * to be used would overwrite what the second still expects to find there. A
 * frame slot can be read any number of times. */
void vdup(void)
{
    Value *top = vsp - 1;

    if (vtop == 0)
        acc_error("internal: nothing to duplicate");
    if (top->kind == VAL_ACC)
        force_reg(vsp - 1);
    if (top->kind == VAL_REG)
        save_regs_below(0);

    top = vsp - 1;
    vpush(top->kind, top->type, top->val);
    (vsp - 1)->ext = top->ext;
    (vsp - 1)->quals = top->quals;
    (vsp - 1)->bits = top->bits;
}

/* ------------------------------------------------------------------ */
/* ++ and --                                                           */

/* The top stops being the name of a variable and becomes a value of its own.
 *
 * A local on the stack is a description of where to find it, and reading it
 * later reads whatever is there later. That is what an operand wants, and not
 * what `x++` wants: its answer is the value x had before it was changed, and
 * the change happens before the answer is used. A long or a float is copied
 * to a scratch slot; anything else is loaded into a register, which is a copy
 * by being somewhere else. */
static void vsnapshot(void)
{
    Value *top = vsp - 1;

    if (top->kind != VAL_LOCAL)
        return;

    if (top->kind == VAL_WIDE)
        wide_needs_slot();
    top = vsp - 1;
    if (type_wide(top->type)) {
        Type type = top->type;
        int slot = long_scratch(type);

        materialise_long(slot, type);
        vdrop();
        vpush_scratch(type, slot);

        return;
    }

    force_reg(vsp - 1);
}

/* What x steps by is 1, however wide x is: a pointer turns it into the width
 * of what it points at, and a float into 1.0, both by the ordinary rules. The
 * narrow destination lets a byte step in A, as `c = c + 1` already may. */
static void vstep(int op, Type type)
{
    vpush_const(1, TY_INT);
    vapply(op, type_narrow(type));
}

/* ++x and --x on a local: x changed, and the answer is its new value. */
void vprefix_local(int offset, Type type, int ext, int op)
{
    gen_effects++;
    vpush_local(offset, type);
    vset_ext(ext);
    vstep(op, type);
    vstore_local(offset, type);
}

/* Whether x++ can be done by stepping back rather than by keeping a copy.
 *
 * The answer x++ wants is the value x had, so the obvious shape reads x
 * twice: once for the answer and once for the sum. Reading a local is six
 * cycles and three bytes, and there is no cheap way to duplicate a register
 * here either -- `ex de, hl` swaps rather than copies, and a push with a pop
 * costs more than the second read did.
 *
 * So: change x, store it, and step back to what it was. One read, and the
 * step back is the same one byte the step forward is.
 *
 * Only where stepping back lands exactly where it started. A type narrower
 * than a register does not: the store truncates, and an unsigned char at 255
 * stores 0, where stepping back gives -1 and not 255. A float does not
 * either, once the value is large enough that adding one to it changes
 * nothing. And the step has to be one of the small ones written out as
 * `inc hl`, or the way back costs more than the read it saved -- which for a
 * pointer means the thing it points at has to be small. */
static int postfix_steps_back(Type type)
{
    if (type_wide(type) || type_float(type) || type_size(type) != ACC_INT_SIZE)
        return 0;
    if (type_pointer(type))
        return type_size(type_deref(type)) <= STEP_MAX;

    return 1;
}

/* x++ and x-- on a local: x changed, and the answer is its old value. */
void vpostfix_local(int offset, Type type, int ext, int op)
{
    gen_effects++;
    if (postfix_steps_back(type)) {
        vpush_local(offset, type);
        vset_ext(ext);
        vstep(op, type);
        vstore_local(offset, type);
        vstep(op == TK_PLUS ? TK_MINUS : TK_PLUS, type);

        return;
    }

    vpush_local(offset, type);
    vsnapshot();
    vpush_local(offset, type);
    vset_ext(ext);
    vstep(op, type);
    vstore_local(offset, type);
    vdrop();
}

/* The same two through a pointer, with the address on the stack.
 *
 * The address is worked out once and used twice, to read and then to write,
 * which is why it is duplicated rather than parsed again: `*p++` changes p,
 * and parsing it twice would change it twice. */
void vprefix_indirect(int op)
{
    gen_effects++;
    Type target = type_deref(vtype());

    vdup();
    vderef();
    vstep(op, target);
    vstore_indirect();
}

void vpostfix_indirect(int op)
{
    gen_effects++;
    Type target = type_deref(vtype());
    Value held;

    vdup();                     /* address, address */
    vderef();                   /* address, old */
    vdup();                     /* address, old, old */
    vstep(op, target);          /* address, old, new */

    /* The old value moved from between the address and the new one to
     * underneath both, which is where the store will leave it: nothing is
     * emitted, a Value being a description of where something is and not
     * the thing. */
    held = *(vsp - 3);
    *(vsp - 3) = *(vsp - 2);
    *(vsp - 2) = held;

    vstore_indirect();          /* old, new */
    vdrop();                    /* old */
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
    int quals = (middle_ext >> 8 | top->quals) & 0xff;
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

/* ------------------------------------------------------------------ */
/* taking code back                                                    */

void gen_mark(GenMark *m)
{
    m->at = out_here();
    m->nfixups = nfixups;
    m->nrt_fixups = nrt_fixups;
    m->nbss_fixups = nbss_fixups;
    m->narray_patches = narray_patches;
    m->spill_used = spill_used;
    m->spill_locked = spill_locked;
    m->nwide_consts = nwide_consts;
    m->vtop = vtop;
    m->rt_any_used = rt_any_used;
    m->saved = NULL;
    if (vtop) {
        m->saved = malloc((size_t) vtop * sizeof *m->saved);
        if (!m->saved)
            acc_error("out of memory for sizeof");
        memcpy(m->saved, vstack, (size_t) vtop * sizeof *m->saved);
    }
}

void gen_rollback(GenMark *m)
{
    /* The image goes back to where it was, so a mark left in what is being
     * forgotten would look like one left just now. */
    cmp_from = -1;
    conversion_from = -1;
    out_rewind(m->at);
    jumps_forget(m->at);
    nfixups = m->nfixups;
    nrt_fixups = m->nrt_fixups;
    nbss_fixups = m->nbss_fixups;
    narray_patches = m->narray_patches;
    spill_used = m->spill_used;
    spill_locked = m->spill_locked;
    nwide_consts = m->nwide_consts;
    vtop = m->vtop;
    vsp = vstack + vtop;
    rt_any_used = m->rt_any_used;
    if (m->saved) {
        memcpy(vstack, m->saved, (size_t) vtop * sizeof *m->saved);
        free(m->saved);
    }
}

void vcast(Type to, int ext, int quals)
{
    if (to == TY_VOID) {
        vdrop();
        vpush(VAL_VOID, TY_VOID, 0);

        return;
    }
    if (vsp[-1].kind == VAL_VOID)
        void_used();
    vconvert(to);
    if (type_pointer(to)) {
        vsp[-1].ext = (unsigned char) ext;
        vsp[-1].quals = (unsigned char) quals;
    }
}
