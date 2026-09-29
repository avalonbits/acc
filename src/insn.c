/*
 * eZ80 instructions: the encoders the rest of the code generator writes
 * through, a local's place in the frame, and loading and storing values
 * narrower than an int.
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
/* instructions                                                        */

/* Z80 numbers a register pair in a two-bit field: BC 0, DE 1, HL 2. acc
 * numbers them HL, DE, BC so that HL -- the one everything returns in -- is
 * register zero. This maps between the two. */
/* Kept already shifted into the bits of an opcode where the register goes.
 * Every use is `reg_code * 0x10` added to a base opcode, and a byte shift by
 * four is a call into the runtime on this target -- once for every register
 * load, store, push and pop the compiler emitted. */
const unsigned char reg_code[NREGS] = { 0x20, 0x10, 0x00 };


/* Where the last ld hl, nn ended: a read through that address straight
 * after it is that instruction made into ld hl, (nn) or ld a, (nn), the
 * operand -- and whatever relocation or fixup names it -- where it was. */
int      imm_hl_end = -1;
unsigned imm_hl_epoch;

void ld_rr_imm(int reg, int imm)    /* ld rr, nn */
{
    out_opcode24(0x01 + reg_code[reg], imm);
    if (reg == R_HL) {
        imm_hl_end = out_here();
        imm_hl_epoch = out_rewinds;
    }
}

/* The last register stored whole to a frame slot, and where the store
 * ended. A load of that slot into that register straight after it -- the
 * end of one statement storing x and the start of the next reading it,
 * which is a twentieth of zap's time -- loads what is there already, so it
 * is left out. Unless something may jump in between the two: join_at is
 * the last place something jumps to (gen_here, and a jump patched to where
 * the code has got to). And not once anything has been taken back. */
int      stored_at = -1, stored_disp, stored_reg, join_at = -1;
unsigned stored_epoch;

void ld_ix_rr(int disp, int reg)    /* ld (ix+d), rr */
{
    if (disp_fits(disp)) {
        out_byte3(0xdd, 0x0f + reg_code[reg], disp);
        stored_at = out_here();
        stored_disp = disp;
        stored_reg = reg;
        stored_epoch = out_rewinds;
    } else {
        far_op(0xfd, 0x0f + reg_code[reg], disp);
    }
}

void push_rr(int reg) { out_byte(0xc5 + reg_code[reg]); }
void pop_rr(int reg)  { out_byte(0xc1 + reg_code[reg]); }

void add_hl_rr(int reg) { out_byte(0x09 + reg_code[reg]); }
void sbc_hl_rr(int reg) { out_byte2(0xed, 0x42 + reg_code[reg]); }
void or_a_a(void)     { out_byte(0xb7); }

void mov_rr(int dst, int src)
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
 * refused before.
 *
 * far_op lays down the whole access: the pointer, then `prefix op d` on it.
 * In a function with a local in IY, IY is saved around the two. */
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

__attribute__((noinline)) void far_op(int prefix, int op, int disp)
{
    iy_save();
    disp = far_base(disp);
    out_byte3(prefix, op, disp);
    iy_restore();
}

/* The result of a void function, used as though it were a value. Found where
 * a value is loaded or converted -- the paths any use of it ends up on --
 * and on the branches of them that read from the frame, which the common
 * cases have already left, so that it costs them nothing. */
__attribute__((noinline, noreturn))
void void_used(void)
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
void struct_used(void)
{
    acc_error_at(tok_line, "a struct or union cannot be used as a number");
}

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
void frame_byte(int op, int disp)
{
    if (disp_fits(disp))
        out_byte3(0xdd, op, disp);
    else
        far_op(0xfd, op, disp);
}

void ld_a_ix(int disp)   { frame_byte(0x7e, disp); }
void ld_e_ix(int disp)   { frame_byte(0x5e, disp); }
void ld_l_ix(int disp)   { frame_byte(0x6e, disp); }
static void ld_h_ix(int disp)   { frame_byte(0x66, disp); }
void ld_ix_a(int disp)   { frame_byte(0x77, disp); }
static void ld_ix_l(int disp)   { frame_byte(0x75, disp); }
static void ld_ix_h(int disp)   { frame_byte(0x74, disp); }
void ld_l_a(void)        { out_byte(0x6f); }
void ld_h_a(void)        { out_byte(0x67); }
void ld_a_l(void)        { out_byte(0x7d); }
void ld_a_h(void)        { out_byte(0x7c); }
static void rlc_l(void)         { out_byte2(0xcb, 0x05); }
void ld_a_hl(void)       { out_byte(0x7e); }      /* ld a, (hl) */
void ld_hl_a(void)       { out_byte(0x77); }      /* ld (hl), a */
void inc_hl(void)        { out_byte(0x23); }
void dec_hl(void)        { out_byte(0x2b); }
void inc_de(void)        { out_byte(0x13); }
void ld_de_a(void)       { out_byte(0x12); }      /* ld (de), a */
void ex_de_hl(void)      { out_byte(0xeb); }
void ld_hl_ind_hl(void)  { out_byte2(0xed, 0x27); }  /* ld hl, (hl) */
void ld_ind_hl_de(void)  { out_byte2(0xed, 0x1f); }  /* ld (hl), de */
void sbc_hl_hl(void)     { out_byte2(0xed, 0x62); }

/* HL = the sign of A, in all three bytes. Clobbers L on the way. */
void fill_hl_with_sign_of_a(void)
{
    ld_l_a();
    rlc_l();                    /* bit 7 into the carry */
    sbc_hl_hl();                /* 0 or -1, upper byte included */
}

void fill_hl_with_zero(void)
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
int and_a_imm(int v)  { out_byte2(0xe6, v & 0xff); return 0; }
int or_a_imm(int v)   { out_byte2(0xf6, v & 0xff); return 0; }
int xor_a_imm(int v)  { out_byte2(0xee, v & 0xff); return 0; }

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
void add_hl_hl(void) { out_byte(0x29); }

/* IY around the backend's own uses of it -- a narrowing, a two-byte load,
 * a long moved three bytes at a time, a slot out of (ix+d)'s reach -- in a
 * function where it holds a local: pushed before and popped after, and
 * nothing at all in any other function. */
void iy_save(void)
{
    if (iy_local)
        out_iy(0xe5);                           /* push iy */
}

void iy_restore(void)
{
    if (iy_local)
        out_iy(0xe1);                           /* pop iy */
}

/* An instruction on IY, and one with a displacement from it: calls rather
 * than the inlined emitters, since each of those is fifty bytes of agondev's
 * code at every place it is written. */
__attribute__((noinline)) void out_iy(int op)
{
    out_byte2(0xfd, op);
}

__attribute__((noinline)) void out_iy_d(int op, int disp)
{
    out_byte3(0xfd, op, disp);
}

/* lea rr, iy+d: the local in IY, plus d, copied into a register -- the
 * register's code, as ld rr, (ix+d) has it, and three. */
void lea_rr_iy(int reg, int disp)
{
    out_byte3(0xed, 0x03 + reg_code[reg], disp);
}

/* HL = HL >> (16 + count), count 0 to 7, written out: push hl; inc sp;
 * pop af; dec sp leaves HL's top byte in A, which is shifted by the rest
 * and widened -- or a, a; sbc hl, hl for zeros above it, or add a, a;
 * sbc hl, hl; rra for its sign, the carry sbc leaves being the one it
 * read -- into ld l, a. Each is a table with all seven shifts, and the ones
 * not wanted are stepped over: the tables are smaller than the code that
 * would put the pieces together. Answers is_unsigned, which is the type of
 * the result too. */
static const unsigned char shr_signed[23] = {
    0xe5, 0x33, 0xf1, 0x3b,
    0xcb, 0x2f, 0xcb, 0x2f, 0xcb, 0x2f, 0xcb, 0x2f,
    0xcb, 0x2f, 0xcb, 0x2f, 0xcb, 0x2f,
    0x87, 0xed, 0x62, 0x1f, 0x6f
};
static const unsigned char shr_unsigned[22] = {
    0xe5, 0x33, 0xf1, 0x3b,
    0xcb, 0x3f, 0xcb, 0x3f, 0xcb, 0x3f, 0xcb, 0x3f,
    0xcb, 0x3f, 0xcb, 0x3f, 0xcb, 0x3f,
    0xb7, 0xed, 0x62, 0x6f
};

int shr_hl_16(int count, int is_unsigned)
{
    const unsigned char *code = is_unsigned ? shr_unsigned : shr_signed;
    unsigned char i, end = is_unsigned ? 22 : 23;

    for (i = 0; i != end; i++) {
        if (i == 4)
            i = (unsigned char) (18 - 2 * count);
        out_byte(code[i]);
    }

    return is_unsigned;
}

/* One more than k where v, as 24 bits, is 2^k; 0 where it is not a power
 * of two. The host's int is wider, and a constant that fills 24 bits is
 * held there as a negative number, so its bits above 24 go. */
static int pow2_rank(int v)
{
    unsigned u = (unsigned) v, m = 1;
    int rank = 1;

#ifndef AGONDEV
    u &= 0xffffff;
#endif
    for (; rank != 25; m += m, rank++)
        if (u == m)
            return rank;

    return 0;
}

/* HL >> c, HL / c or HL % c, for the constant c in `rhs`, done without
 * the helper's loop where it can be. Answers the result's type if it was,
 * 1 if the remainder is an AND with rhs, which it has made one less, and 0
 * if neither. A shift is unsigned by its left operand, the others by
 * either, each promoted.
 *
 * By 16 to 23 is HL's top byte (shr_hl_16), for a shift or an unsigned
 * divide. By 1 to 8 is the runtime's shr, which shifts A:HL left by 8 - k
 * and keeps the top three bytes, or for a signed divide its sdiv, which
 * rounds toward zero as division does: a call and no count, where the
 * helpers loop. A holds what fills from the top: xor a for zeros, and
 * push hl; add hl, hl; pop hl; sbc a, a for the sign. An unsigned
 * remainder by 2^k is the bits below it. */
int shr_const(int op, const Value *lhs, Value *rhs)
{
    int v = rhs->val;
    int is_unsigned = type_unsigned(type_promote(lhs->type));
    unsigned k1 = (unsigned) v - 1;             /* k - 1 */
    unsigned char which = RT_SHR;

    /* A divide by 1, or by no power of two, is out of every range. */
    if (op == TK_SLASH || op == TK_PERCENT) {
        int rank = pow2_rank(v);

        is_unsigned |= type_unsigned(type_promote(rhs->type));
        k1 = (unsigned) rank - 2;
        if (op == TK_PERCENT) {
            if (!is_unsigned || !rank)
                return 0;
            rhs->val = v - 1;

            return 1;
        }
        if (!is_unsigned)
            which = RT_SDIV;
    } else if (op != TK_SHR) {
        return 0;
    }
    if (k1 - 15 < 8 && which == RT_SHR) {
        shr_hl_16((int) k1 - 15, is_unsigned);
    } else {
        if (k1 >= 8)
            return 0;
        if (!is_unsigned)
            out_word24(0xe129e5);
        out_byte(is_unsigned ? 0xaf : 0x9f);

        /* One routine each, entered 2(k - 1) bytes in: the slot holds
         * that, and the link adds the routine's address to it. */
        rt_call(which);
        out_put[-ACC_INT_SIZE] = (unsigned char) (k1 + k1);
    }

    return is_unsigned ? TY_UINT : TY_INT;
}

/* The powers of two a constant multiplier can hold, lowest first. */
const unsigned powers_of_two[16] = {
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
    0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000
};

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
void load_narrow_into(int reg, int disp, Type type)
{
    if (reg == R_HL) {
        load_narrow(disp, type);

        return;
    }

    /* An unsigned one goes straight in: the pair cleared, and the bytes
     * loaded into its low halves, which have names in DE and BC as they
     * do in HL. Only the sign fill needs HL. `t[c]` with the table's
     * address in HL loaded c into DE this way where it went through HL and
     * the stack. */
    if (type_unsigned(type)) {
        static const unsigned char low[] = { 0, 0x5e, 0x4e };   /* ld e / c */
        static const unsigned char high[] = { 0, 0x56, 0x46 };  /* ld d / b */

        ld_rr_imm(reg, 0);
        out_byte3(0xdd, low[reg], disp);
        if (type_size(type) == 2)
            out_byte3(0xdd, high[reg], disp + 1);

        return;
    }
    push_rr(R_HL);
    load_narrow(disp, type);
    push_rr(R_HL);
    pop_rr(reg);
    pop_rr(R_HL);
}

/* Store the low bytes of HL into a local of the given type. */
void store_narrow(int disp, Type type)
{
    if (type_size(type) == 1) {
        ld_a_l();
        ld_ix_a(disp);

        return;
    }
    ld_ix_l(disp);
    ld_ix_h(disp + 1);
}
