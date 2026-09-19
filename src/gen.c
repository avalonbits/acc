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

static void ld_rr_imm(int reg, int imm)    /* ld rr, nn */
{
    out_opcode24(0x01 + reg_code[reg], imm);
}

static inline __attribute__((always_inline))
void ld_rr_ix(int reg, int disp)           /* ld rr, (ix+d) */
{
    out_byte3(0xdd, 0x07 + reg_code[reg], disp);
}

static void ld_ix_rr(int disp, int reg)    /* ld (ix+d), rr */
{
    out_byte3(0xdd, 0x0f + reg_code[reg], disp);
}

static void push_rr(int reg) { out_byte(0xc5 + reg_code[reg]); }
static void pop_rr(int reg)  { out_byte(0xc1 + reg_code[reg]); }

static void add_hl_rr(int reg) { out_byte(0x09 + reg_code[reg]); }
static void sbc_hl_rr(int reg) { out_byte2(0xed, 0x42 + reg_code[reg]); }
static void or_a_a(void)     { out_byte(0xb7); }

/* There is no ld rr, rr' on this chip. Always through the stack and never
 * `ex de, hl`: that is a byte shorter but it swaps, and the register
 * allocator is entitled to believe the source still holds what it held. */
static void mov_rr(int dst, int src)
{
    if (dst == src)
        return;
    push_rr(src);
    pop_rr(dst);
}

/* (ix+d) carries one signed byte of displacement. A frame that outgrows it
 * needs the address computed instead, which is a real cost on every access;
 * for now it is refused rather than paid for silently. */
/* Blamed on the line being compiled, which is where the local that went too
 * far was declared or first touched -- the message used to carry no line at
 * all, and a program with several functions gave no hint which one it
 * meant. */
__attribute__((noinline))
static void disp_too_far(int d)
{
    acc_error_at(tok_line, "this function's frame is too large: a local at "
                           "%d is out of reach of (ix+d), which spans -128 "
                           "to 127", d);
}

/* That a frame offset fits the signed byte of (ix+d). Tested inline, because
 * it runs for every local touched and the call to test it opened a frame;
 * as one unsigned compare rather than two signed ones, because a signed
 * compare on this target is a call to repair the flags. */
#define need_disp(d)  do { if ((unsigned) ((d) + 128) > 255u) disp_too_far(d); } while (0)

static int  spill_slot(void);
static int  force_reg(Value *val);

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

static int  long_scratch(void);
static void check_no_float_mix(Type to, const Value *from);
static void materialise_long(int disp, Type type);
static void evict_reg(int reg);
static void vunary_long(int which, Type type);
static void vswap(void);
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

static void ld_a_ix(int disp)   { out_byte3(0xdd, 0x7e, disp); }
static void ld_e_ix(int disp)   { out_byte3(0xdd, 0x5e, disp); }
static void ld_l_ix(int disp)   { out_byte3(0xdd, 0x6e, disp); }
static void ld_h_ix(int disp)   { out_byte3(0xdd, 0x66, disp); }
static void ld_ix_a(int disp)   { out_byte3(0xdd, 0x77, disp); }
static void ld_ix_l(int disp)   { out_byte3(0xdd, 0x75, disp); }
static void ld_ix_h(int disp)   { out_byte3(0xdd, 0x74, disp); }
static void ld_l_a(void)        { out_byte(0x6f); }
static void ld_h_a(void)        { out_byte(0x67); }
static void ld_a_l(void)        { out_byte(0x7d); }
static void ld_a_h(void)        { out_byte(0x7c); }
static void rlc_l(void)         { out_byte2(0xcb, 0x05); }
static void ld_a_hl(void)       { out_byte(0x7e); }      /* ld a, (hl) */
static void ld_hl_a(void)       { out_byte(0x77); }      /* ld (hl), a */
static void inc_hl(void)        { out_byte(0x23); }
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

#define VSTACK_MAX 64

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
static int spill_peak;           /* the most it ever held */
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
        acc_error("expression is nested too deeply");
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
    vsp++;
    vtop++;
}

void vpush_const(int val, Type type)
{
    vpush(VAL_CONST, type, val);
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

    /* Between a float and an integer is a conversion of the value, not of
     * the label on it. Both directions go through an int, so a narrow type
     * widens first and a long is still refused. */
    if (type_float(to) != type_float(top->type)
        && !(top->kind == VAL_CONST && top->val == 0)) {
        if (type_float(to)) {
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

        if (top->kind == VAL_LOCAL && type_wide(top->type)) {
            top->type = to;     /* already four bytes in the frame */

            return;
        }
        slot = long_scratch();
        need_disp(slot);
        need_disp(slot + ACC_LONG_SIZE - 1);
        materialise_long(slot, to);
        vdrop();
        vpush(VAL_LOCAL, to, slot);

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

    if (top->kind == VAL_CONST) {
        int bits = type_size(to) * 8;
        int mask = (1 << bits) - 1;

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

/* Whether the top of the stack is a constant, and if so its value and type.
 * What a global's initial value has to come to. */
int vconst_top(int *val, Type *type)
{
    const Value *top = vsp - 1;

    if (vtop == 0 || top->kind != VAL_CONST)
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
}

/* Always inlined, for the same reason: the allocator asks it about every
 * register it considers, and the frame cost more than the loop. */
static inline __attribute__((always_inline))
int reg_busy(int reg)
{
    /* Unsigned for the same reason as everywhere else here: a signed `<` is
     * a helper call to repair the flags, and this runs for every register
     * the allocator considers. */
    unsigned i, n = (unsigned) vtop;

    for (i = 0; i < n; i++)
        if (vstack[i].kind == VAL_REG && vstack[i].val == reg)
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
    int i;

    for (i = 0; i < vtop; i++) {
        if (vstack[i].kind == VAL_REG && vstack[i].val != avoid) {
            int off = spill_slot();

            need_disp(off);
            ld_ix_rr(off, vstack[i].val);
            vstack[i].kind = VAL_LOCAL;
            vstack[i].val = off;

            return;
        }
    }
    acc_error("internal: nothing to spill");
}

/* Spills every register-held value except the top n, which the caller is
 * about to consume. */
static void save_regs_below(int n)
{
    int i;

    for (i = 0; i < vtop - n; i++) {
        if (vstack[i].kind == VAL_REG) {
            int off = spill_slot();

            need_disp(off);
            ld_ix_rr(off, vstack[i].val);
            vstack[i].kind = VAL_LOCAL;
            vstack[i].val = off;
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
         * and gets the promoted value, which is what C says it should see. */
        reg = R_HL;
        evict_reg(R_HL);
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
    if (val->kind == VAL_CONST) {
        ld_rr_imm(reg, val->val);
    } else {
        if (val->kind == VAL_VOID)
            void_used();
        need_disp(val->val);
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
    need_disp(to);
    ld_ix_rr(to, entry->val);
    entry->kind = VAL_LOCAL;
    entry->val = to;
}

/* Moves every value out of `reg`, so that it can be written without losing
 * anything the expression still needs. */
static void evict_reg(int reg)
{
    int i;

    for (i = 0; i < vtop; i++)
        if (vstack[i].kind == VAL_REG && vstack[i].val == reg)
            move_out(&vstack[i]);
}

/* Materialises the entry at `depth` into one particular register, moving
 * whatever else is living there out of the way first. */
/* Named by pointer rather than by depth from the top. Every caller says
 * `vsp - 1` or `vsp - 2`, which is a constant offset; a depth has to be
 * turned into an address, and scaling an index is a helper call here. */
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
        if (want != R_HL)
            evict_reg(R_HL);
        if (type_unsigned(target->type))
            fill_hl_with_zero();
        else
            fill_hl_with_sign_of_a();
        ld_l_a();
        if (want != R_HL)
            mov_rr(want, R_HL);
    } else if (target->kind == VAL_REG) {
        if (target->val != want)
            mov_rr(want, target->val);
    } else if (target->kind == VAL_CONST) {
        ld_rr_imm(want, target->val);
    } else {
        if (target->kind == VAL_VOID)
            void_used();
        need_disp(target->val);
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

static int const_fold(int op, int left, int right, int *out)
{
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
        *out = trunc_int(left << right);
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

static void vbinop(int op)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;
    int folded, right;
    Type result, lhs_type;

    if ((unsigned) vtop < 2)
        acc_error("internal: binary operator with nothing to work on");

    /* Both sides known: the answer is known, and nothing is emitted. */
    if (lhs->kind == VAL_CONST && rhs->kind == VAL_CONST
        && const_fold(op, lhs->val, rhs->val, &folded)) {
        Type folded_type = either_unsigned(lhs, rhs) ? TY_UINT : TY_INT;

        vdrop();
        vdrop();
        vpush_const(folded, folded_type);

        return;
    }

    /* Adding or subtracting nothing is nothing. Worth the two lines: it is
     * what makes `p + 0` and the zero cases of generated code free. */
    if (rhs->kind == VAL_CONST && rhs->val == 0
        && (op == TK_PLUS || op == TK_MINUS)) {
        vdrop();

        return;
    }

    /* add hl, rr and sbc hl, rr only accumulate into HL, so the left operand
     * goes there and the right one goes anywhere else. Both are done through
     * the allocator rather than by moving registers about by hand: a scratch
     * register chosen without asking whether anything already lives in it is
     * how `f(a,b,c) + f(1,2,3)` lost an argument. */
    force_into(vsp - 2, R_HL);
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
void vstore_local(int offset, Type type)
{
    int reg;

    /* An assignment converts the value to the type of the object, and
     * between a float and an integer that is arithmetic rather than a
     * relabelling. vconvert is where it lives; this used to refuse instead,
     * which is why a float could be stored and read back but never made from
     * anything. */
    if (type_float(type) != type_float((vsp - 1)->type))
        vconvert(type);

    if (type_wide(type)) {
        need_disp(offset);
        need_disp(offset + ACC_LONG_SIZE - 1);
        materialise_long(offset, type);
        vdrop();
        vpush(VAL_LOCAL, type, offset);

        return;
    }

    if ((vsp - 1)->kind == VAL_ACC && (vsp - 1)->type == type) {
        /* Already in A at its own width, which is where a byte store reads
         * from. Nothing to convert and nothing to move. */
        need_disp(offset);
        ld_ix_a(offset);

        return;
    }

    if (type_size(type) < ACC_INT_SIZE) {
        /* The narrow stores write out of HL, so the value goes there. */
        force_into(vsp - 1, R_HL);
        vconvert(type);
        need_disp(offset);
        store_narrow(offset, type);

        return;
    }

    reg = force_reg(vsp - 1);
    need_disp(offset);
    ld_ix_rr(offset, reg);
}

void vneg(void)
{
    Value *top = vsp - 1;
    int right;

    if (type_float(top->type)) {
        /* Negating a float is its sign bit flipped and nothing else -- no
         * routine, and correct for zero and for every other value alike. */
        int slot = long_scratch();

        save_regs_below(1);
        materialise_long(slot, top->type);
        vdrop();

        need_disp(slot + ACC_LONG_SIZE - 1);
        ld_a_ix(slot + ACC_LONG_SIZE - 1);
        out_byte2(0xee, 0x80);          /* xor a, 0x80 */
        ld_ix_a(slot + ACC_LONG_SIZE - 1);
        vpush(VAL_LOCAL, top->type, slot);

        return;
    }

    if (type_wide(top->type)) {
        vunary_long(RT_LNEG, top->type);

        return;
    }

    if (top->kind == VAL_CONST) {
        top->val = trunc_int(-top->val);

        return;
    }

    /* 0 - x, so the operand goes anywhere but HL and HL is then cleared of
     * whatever else was in it -- the zero is about to overwrite it. */
    if (!(top->kind == VAL_REG && top->val != R_HL))
        force_into(top, reg_alloc_other(R_HL));
    right = top->val;
    evict_reg(R_HL);

    ld_rr_imm(R_HL, 0);
    or_a_a();
    sbc_hl_rr(right);
    vdrop();
    vpush_reg(R_HL);
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

    if (type_wide(top->type)) {
        vunary_long(RT_LNOT, top->type);

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
} Fixup;

static Fixup *fixups;
static int    nfixups, fixups_cap;

static void fixup_add(int fn, int at)
{
    if (nfixups == fixups_cap) {
        fixups_cap = fixups_cap ? fixups_cap * 2 : 32;
        fixups = realloc(fixups, fixups_cap * sizeof *fixups);
        if (!fixups)
            acc_error("out of memory for forward calls");
    }
    fixups[nfixups].fn = fn;
    fixups[nfixups].at = at;
    fixups[nfixups].line = tok_line;
    nfixups++;
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
static void ld_a_ix_b(int disp)  { out_byte3(0xdd, 0x7e, disp); }

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
    if (val->kind == VAL_CONST)
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
        if (rhs->kind != VAL_CONST || rhs->val < 0 || rhs->val > 8)
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
    if (lhs->kind == VAL_CONST)
        ld_a_imm(lhs->val);
    else if (lhs->kind == VAL_LOCAL)
        ld_a_ix_b(lhs->val);

    if (tok_pair(op, TK_SHL)) {
        int count = rhs->val;

        while (count-- > 0)
            shift_a_once(op, to);
    } else if (rhs->kind == VAL_CONST) {
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

static int jump_op(int op);
static void patch_to_here(int hole);

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

    if (lhs->kind == VAL_CONST && rhs->kind == VAL_CONST
        && !is_unsigned
        && const_fold(op, lhs->val, rhs->val, &folded)) {
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

    switch (op) {
    case TK_EQ: cmp_equal(1); break;
    case TK_NE: cmp_equal(0); break;
    case TK_LT: if (is_unsigned) cmp_unsigned(1); else cmp_signed(1); break;
    case TK_GE: if (is_unsigned) cmp_unsigned(0); else cmp_signed(0); break;
    default:
        acc_error("internal: %s is not a comparison", tok_spelling(op));
    }

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
 * On the stack a long is therefore always VAL_LOCAL. Anything that forces it
 * into a register is asking for the int it converts to, which is its low
 * three bytes -- so force_reg does exactly that and needs no special case.
 */

static int spill_slot_of(int size);

/* lea rr, ix+d -- the address of a frame slot, which is what the helpers take.
 * The second byte is the register, and these are the assembler's own numbers
 * rather than a reading of the opcode map: guessing DE cost a debugging pass. */
static void lea_rr_ix(int reg, int disp)
{
    static const unsigned char lea_code[NREGS] = { 0x22, 0x12, 0x02 };

    out_byte3(0xed, lea_code[reg], disp);
}

/* Copy four bytes from one frame slot to another. */
static void copy_long(int to, int from)
{
    int i;

    for (i = 0; i < ACC_LONG_SIZE; i++) {
        ld_a_ix(from + i);
        ld_ix_a(to + i);
    }
}

/* Write an int-wide value, already in HL, into a long slot: three bytes and
 * then the byte the sign or the zero extension calls for. */
static void store_int_as_long(int disp, int is_unsigned)
{
    need_disp(disp);
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
    ld_ix_a(disp + ACC_INT_SIZE);
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
        slot = long_scratch();
        materialise_long(slot, top->type);
        vdrop();

        need_disp(slot);
        need_disp(slot + ACC_LONG_SIZE - 1);
        lea_rr_ix(R_HL, slot);
        rt_call(unsign ? RT_ULTOF : RT_LTOF);
        vpush(VAL_LOCAL, TY_FLOAT, slot);

        return;
    }

    force_into(top, R_HL);

    slot = long_scratch();
    need_disp(slot);
    need_disp(slot + ACC_LONG_SIZE - 1);
    lea_rr_ix(R_DE, slot);
    rt_call(unsign ? RT_UITOF : RT_ITOF);
    vdrop();
    vpush(VAL_LOCAL, TY_FLOAT, slot);
}

static void convert_float_to_int(Type to)
{
    int slot;

    save_regs_below(1);

    /* The routine takes an address, so a float that is not already in the
     * frame has to be put there. Every float is, as it happens -- four bytes
     * do not fit in a register -- but materialise_long is what says so. */
    slot = long_scratch();
    materialise_long(slot, TY_FLOAT);
    vdrop();

    need_disp(slot);
    need_disp(slot + ACC_LONG_SIZE - 1);
    lea_rr_ix(R_HL, slot);

    /* A long stays in the frame, where the routine rewrites it in place. */
    if (type_wide(to)) {
        rt_call(RT_FTOL);
        vpush(VAL_LOCAL, to, slot);

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

/* The bytes of a float and of an integer mean different things, so moving a
 * value between them is arithmetic and not a copy. Every path that widens or
 * stores four bytes comes through here, which is why the check lives here
 * rather than at each of them. */
static void check_no_float_mix(Type to, const Value *from)
{
    if (type_float(to) == type_float(from->type))
        return;
    if (from->kind == VAL_CONST && from->val == 0)
        return;                 /* zero is all zero bits either way */

    acc_error_at(tok_line, "converting between floating-point and integer is "
                           "not implemented yet");
}

/* Put the top of the stack into a long slot, whatever width it arrived as. */
static void materialise_long(int disp, Type type)
{
    Value *top = vsp - 1;

    check_no_float_mix(type, top);

    if (top->kind == VAL_LOCAL && type_wide(top->type)) {
        copy_long(disp, top->val);

        return;
    }
    force_into(top, R_HL);
    store_int_as_long(disp, type_unsigned(top->type));
}

/* A floating constant, laid down as the four bytes the machine reads. The
 * host's float is the same IEEE 754 single this target uses, so the bits are
 * taken from it rather than assembled: anything else would be a second
 * implementation of the format, to be got wrong separately. */
void vpush_const_float(float val)
{
    int slot = spill_slot_of(ACC_LONG_SIZE);
    unsigned char bytes[ACC_LONG_SIZE];
    int i;

    memcpy(bytes, &val, ACC_LONG_SIZE);
    need_disp(slot);
    need_disp(slot + ACC_LONG_SIZE - 1);
    for (i = 0; i < ACC_LONG_SIZE; i++) {
        out_byte2(0x3e, bytes[i]);              /* ld a, n */
        ld_ix_a(slot + i);
    }
    vpush(VAL_LOCAL, TY_FLOAT, slot);
}

/* A constant too wide for a register goes straight to a frame slot, which is
 * where every long lives. */
void vpush_const_long(long val, Type type)
{
    int slot = spill_slot_of(ACC_LONG_SIZE);
    int i;

    need_disp(slot);
    need_disp(slot + ACC_LONG_SIZE - 1);
    for (i = 0; i < ACC_LONG_SIZE; i++) {
        out_byte(0x3e);                         /* ld a, n */
        out_byte((int) ((val >> (i * 8)) & 0xff));
        ld_ix_a(slot + i);
    }
    vpush(VAL_LOCAL, type, slot);
}

/* One scratch long, for the left operand of an operation to be built in and
 * overwritten by the result. */
static int long_scratch(void)
{
    return spill_slot_of(ACC_LONG_SIZE);
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

static int long_helper(int op, Type type)
{
    if (type_float(type))
        return float_helper(op);

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

    slot = long_scratch();
    materialise_long(slot, type);
    vdrop();

    need_disp(slot);
    need_disp(slot + ACC_LONG_SIZE - 1);
    lea_rr_ix(R_HL, slot);
    rt_call(which);

    vpush(VAL_LOCAL, type, slot);
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
    save_regs_below(2);

    which = long_helper(op, result);
    if (which < 0)
        acc_error_at(tok_line, "the operator %s is not implemented for %s yet",
                     tok_spelling(op), type_float(result) ? "float" : "long");

    /* The right operand first, because building the left one may need HL and
     * the right may still be an expression on the stack. */
    right = long_scratch();
    materialise_long(right, result);
    vdrop();

    left = long_scratch();
    materialise_long(left, result);
    vdrop();

    need_disp(left);
    need_disp(right);
    lea_rr_ix(R_HL, left);
    lea_rr_ix(R_DE, right);
    rt_call(which);

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
    int left, right;

    /* Anything else live in a register has to come out first. The two lea
     * instructions below load HL and DE with addresses, behind the register
     * allocator's back -- it is not told, because these are not values it
     * will ever be asked for. A result of an earlier operator sitting in DE
     * was overwritten by the second of them, so `(a == 1) + (b == 2)` lost
     * the first comparison. The top two are the operands and are exempt:
     * they are about to be copied into the frame and dropped. */
    save_regs_below(2);

    right = long_scratch();
    materialise_long(right, operand);
    vdrop();

    left = long_scratch();
    materialise_long(left, operand);
    vdrop();

    need_disp(left);
    need_disp(right);

    /* A float comparison answers in four ways and not three, so it does not
     * go through the same tail as the integer ones. */
    if (floating) {
        lea_rr_ix(R_HL, left);
        lea_rr_ix(R_DE, right);
        rt_call(RT_FCMP);
        cmp_from_code(op);
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
        rt_call(RT_LCMPEQ);
        cmp_equal(op == TK_EQ);
    } else {
        /* The last subtract of the four leaves S, P/V and C describing the
         * whole width, so the same branch sequence the 24-bit comparisons use
         * reads them unchanged. */
        rt_call(RT_LCMPORD);

        /* A key is unsigned by construction: that is what makes the negative
         * floats sort below the positive ones. */
        if (floating || type_unsigned(operand))
            cmp_unsigned(op == TK_LT);
        else
            cmp_signed(op == TK_LT);
    }
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

static int jump_op(int op)
{
    int hole;

    out_opcode24(op, 0);
    hole = out_here() - ACC_INT_SIZE;

    return hole;
}

static void patch_to_here(int hole)
{
    out_patch24(hole, out_here());
}

int gen_jump(void)
{
    return jump_op(JP_ANY);
}

void gen_jump_to(int target)
{
    out_opcode24(JP_ANY, target);
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
static int jump_on_truth(int when_true)
{
    int reg;

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

    out_patch24(jump_on_truth(1), target);
}

/* The value a switch compares its cases with, into HL -- and for a long its
 * top byte into A -- once, ahead of all the tests. */
void gen_switch_load(int slot, Type type)
{
    need_disp(slot);
    ld_rr_ix(R_HL, slot);
    if (type_wide(type)) {
        need_disp(slot + ACC_INT_SIZE);
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
void gen_switch_case(long value, Type type, int target)
{
    if (type_wide(type)) {
        out_byte2(0xfe, (int) ((unsigned long) value >> 24) & 0xff);  /* cp n */
        out_byte2(0x20, 12);                    /* jr nz, past the rest */
    }
    out_byte(0x11);                             /* ld de, value */
    out_word24((int) (value & 0xffffff));
    or_a_a();
    sbc_hl_rr(R_DE);
    add_hl_rr(R_DE);
    out_opcode24(JP_Z, target);
}

/* The top becomes 0 or 1 according to how it compares with zero -- TK_NE for
 * its truth, TK_EQ for `!`. The zero is of the value's own kind, so the
 * comparison is a float one for a float and a four-byte one for a long, and a
 * pointer compares with the null pointer as the unsigned int it is. */
void vtruth(int op)
{
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
 * Agon's 448 KB is the cheaper trade. */
static int rt_base = 0;                 /* where the blob landed */
static int rt_any_used;

typedef struct {
    unsigned char which;
    int at;
} RtFixup;

static RtFixup *rt_fixups;
static int      nrt_fixups, rt_fixups_cap;

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
    rt_any_used = 1;

    out_opcode24(0xcd, 0);                       /* call nn */
    rt_fixups[nrt_fixups].which = (unsigned char) which;
    rt_fixups[nrt_fixups].at = out_here() - ACC_INT_SIZE;
    nrt_fixups++;
}

static void rt_emit_used(void)
{
    int i;

    if (!rt_any_used)
        return;

    rt_base = out_here();
    for (i = 0; i < (int) sizeof rt_code; i++)
        out_byte(rt_code[i]);

    /* The calls the routines make to each other, now that the blob has an
     * address. */
    for (i = 0; i < RT_NFIX; i++)
        out_patch24(rt_base + rt_fix[i].at, rt_base + rt_fix[i].to);

    /* And the calls the compiled program makes to them. */
    for (i = 0; i < nrt_fixups; i++)
        out_patch24(rt_fixups[i].at, rt_base + rt_entry[rt_fixups[i].which]);
}

void gen_finish(void)
{
    int i;

    for (i = 0; i < nfixups; i++) {
        Sym *fn = sym_at(fixups[i].fn);

        if (!fn->val)
            acc_error("'%s' is called but never defined", name_text(fn->name));

        /* The call was emitted before the definition was read, so it took C's
         * word that an undeclared function returns int -- and read its answer
         * from HL. A one-byte return comes back in A instead, which that call
         * cannot know. There are no prototypes yet, so the only honest thing
         * is to say so. */
        /* And a void one conflicts with that int outright, as agondev
         * says: it is the same function declared two ways. */
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
        out_patch24(fixups[i].at, fn->val);
    }

    rt_emit_used();
}


void gen_init(void)
{
    vtop = 0;
    vsp = vstack;
}

/* The first thing in the image, because MOS enters at its first byte.
 *
 * The bytes come from src/rt/startup.s, assembled and copied in rather than
 * hand-encoded, so that the source of truth is assembly anyone can read and
 * reassemble. Two versions:
 *
 *   print  calls main, writes the result as six hex digits and returns to
 *          MOS. The default, because it is the one that works at a command
 *          prompt and on a real Agon.
 *   exit   calls main and hands the low byte to IO port 0, which stops the
 *          emulator with that byte as its exit status. That is how the tests
 *          read an answer with no C library and nothing to print with.
 */
static const unsigned char startup_exit[] = {
    0xcd, 0x00, 0x00, 0x00, 0x7d, 0xd3, 0x00, 0xc9
};

static const unsigned char startup_print[] = {
    0xcd, 0x00, 0x00, 0x00, 0xe5, 0xfd, 0x21, 0x00, 0x00, 0x00, 0xfd, 0x39,
    0xfd, 0x7e, 0x02, 0xcd, 0x33, 0x00, 0x00, 0xfd, 0x7e, 0x01, 0xcd, 0x33,
    0x00, 0x00, 0xfd, 0x7e, 0x00, 0xcd, 0x33, 0x00, 0x00, 0xe1, 0x3e, 0x0d,
    0x5b, 0xd7, 0x3e, 0x0a, 0x5b, 0xd7, 0xc9, 0xf5, 0x1f, 0x1f, 0x1f, 0x1f,
    0xcd, 0x3d, 0x00, 0x00, 0xf1, 0xe6, 0x0f, 0xc6, 0x30, 0xfe, 0x3a, 0x38,
    0x02, 0xc6, 0x07, 0x5b, 0xd7, 0xc9
};

/* Where the print stub calls within itself, as offsets from its first byte.
 * They are absolute calls, so they have to be filled in once the stub's
 * address is known. */
static const struct { int at, to; } print_calls[] = {
    { 0x10, 0x2b }, { 0x17, 0x2b }, { 0x1e, 0x2b },   /* hexbyte */
    { 0x31, 0x35 }                                    /* hexnib */
};

void gen_startup(int report_by_exit)
{
    int m = sym_push(name_intern("main", 4), SYM_FUNC, 0);
    const unsigned char *stub = report_by_exit ? startup_exit : startup_print;
    int n = report_by_exit ? (int) sizeof startup_exit : (int) sizeof startup_print;
    int base = out_here();
    int i;

    for (i = 0; i < n; i++)
        out_byte(stub[i]);

    /* The call to main is the first instruction in either version. */
    fixup_add(m, base + 1);

    if (!report_by_exit)
        for (i = 0; i < (int) (sizeof print_calls / sizeof *print_calls); i++)
            out_patch24(base + print_calls[i].at, base + print_calls[i].to);
}

int gen_local(int size)
{
    locals_size += size;

    return -locals_size;
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
static int in_function;

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

/* A slot for a spilled register, which lasts until the end of the statement.
 * They are handed out in order and all released together, so this is a
 * high-water mark and not a free list -- there is nothing to free, since the
 * whole area goes at once. */
static int spill_slot_of(int size)
{
    spill_used += size;
    if (spill_used > spill_peak)
        spill_peak = spill_used;

    return -(locals_size + spill_used);
}

static int spill_slot(void)
{
    spill_used += ACC_INT_SIZE;
    if (spill_used > spill_peak)
        spill_peak = spill_used;

    return -(locals_size + spill_used);
}

void gen_func_begin(int fn, int nparams, Type returns)
{
    return_type = returns;
    return_ext = sym_at(fn)->ext;

    (void) nparams;

    sym_at(fn)->val = out_here();
    vtop = 0;
    vsp = vstack;
    locals_size = 0;
    spill_used = 0;
    spill_peak = 0;
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
             * loads. */
            vconvert(return_type);
            need_disp((vsp - 1)->val);
            need_disp((vsp - 1)->val + ACC_LONG_SIZE - 1);
            ld_rr_ix(R_HL, (vsp - 1)->val);
            ld_e_ix((vsp - 1)->val + ACC_INT_SIZE);
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
void gen_call(int fn, int nargs, int params_first, int nparams)
{
    const Sym *callee;
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
        if (which < nparams) {
            Type param = sym_param_type(params_first, which);

            if (type_is_struct(param))
                struct_argument(param, sym_param_ext(params_first, which),
                                which);
            vconvert(param);
        }

        if (type_wide(vtype())) {
            /* Two slots, six bytes, which is what agondev gives a long. The
             * high half goes first because the stack grows downwards, so the
             * low bytes end up at the lower address. */
            int slot = (vsp - 1)->val;

            need_disp(slot);
            need_disp(slot + ACC_LONG_SIZE - 1);
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

    /* The callee fetched once rather than at each of the six places below
     * that want something from it; nothing between here and the end pushes a
     * symbol, so the pointer stays good. */
    callee = sym_at(fn);

    /* A struct comes back in a temporary the caller provides, in the array
     * area, whose address is passed ahead of the arguments. */
    if (type_is_struct(callee->type)) {
        int temp = gen_local_array(), x = callee->ext;

        gen_local_array_size(temp, ext_bytes(x));
        vaddr_array(temp, TY_CHAR);
        push_rr(vpop_reg());
        argslots++;
        callee = sym_at(fn);
    }

    if (callee->val) {
        out_opcode24(0xcd, callee->val);         /* call nn */
    } else {
        /* Defined further down the file, or not at all. The site is recorded
         * and filled in once the whole file has been read; gen_finish says so
         * if it never was. */
        out_opcode24(0xcd, 0);
        fixup_add(fn, out_here() - ACC_INT_SIZE);
    }

    for (i = 0; i < argslots; i++)
        pop_rr(R_DE);                            /* discard, cheapest form */

    if (type_wide(callee->type)) {
        /* HL with the high byte in E; put it where every long lives. */
        int slot = spill_slot_of(ACC_LONG_SIZE);

        need_disp(slot);
        need_disp(slot + ACC_LONG_SIZE - 1);
        ld_ix_rr(slot, R_HL);
        out_byte(0x7b);                          /* ld a, e */
        ld_ix_a(slot + ACC_INT_SIZE);
        vpush(VAL_LOCAL, callee->type, slot);

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
    (vsp - 1)->ext = callee->ext;
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
    if (type_pointer(left) && type_pointer(right) && left != right)
        acc_error_at(tok_line, "these are pointers to different types");
}

static void vbinop_pointer(int op, Type left, Type right)
{
    int both = type_pointer(left) && type_pointer(right);
    Type ptr = type_pointer(left) ? left : right;
    int ext = type_pointer(left) ? (vsp - 2)->ext : (vsp - 1)->ext;
    int step;

    if (type_deref(ptr) == TY_VOID)
        acc_error_at(tok_line, "a 'void *' does not say what it points at, so "
                               "arithmetic on it has no step to take");
    step = type_step(ptr, ext);

    if (both) {
        if (op != TK_MINUS)
            acc_error_at(tok_line, "%s does not take two pointers",
                         tok_spelling(op));
        if (left != right || (vsp - 2)->ext != (vsp - 1)->ext)
            acc_error_at(tok_line, "these are pointers to different types");

        vbinop(TK_MINUS);
        if ((unsigned) step > 1u) {
            vpush_const(step, TY_INT);
            vbinop(TK_SLASH);
        }
        (vsp - 1)->type = TY_INT;

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
        vbinop(TK_STAR);
    }
    vbinop(op);
    (vsp - 1)->type = ptr;
    (vsp - 1)->ext = (unsigned char) ext;
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
    if ((type_wide(left) && type_unsigned(left))
        || (type_wide(right) && type_unsigned(right)))
        return TY_ULONG;

    return TY_LONG;
}

/* The top two values exchanged. Nothing is emitted: a Value says where a
 * value is, not where it is on this stack, so swapping two of them is a
 * swap of two descriptors. */
static void vswap(void)
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
    need_disp(offset);
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
    int deref_ext = 0;

    if (vtop == 0)
        acc_error("internal: nothing to dereference");
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
        if (top->kind == VAL_CONST)
            force_reg(top);
        top->type = TY_STRUCT;

        return;
    }

    /* A pointer read through a pointer keeps its extension: the chain is
     * one level shorter, and its bottom is the same. */
    if (type_pointer(to))
        deref_ext = top->ext;

    force_into(top, R_HL);

    if (type_wide(to)) {
        int slot = long_scratch();
        int i;

        need_disp(slot);
        need_disp(slot + ACC_LONG_SIZE - 1);
        for (i = 0; i < ACC_LONG_SIZE; i++) {
            ld_a_hl();
            ld_ix_a(slot + i);
            if (i < ACC_LONG_SIZE - 1)
                inc_hl();
        }
        vdrop();
        vpush(VAL_LOCAL, to, slot);

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
    if (type_pointer(to))
        (vsp - 1)->ext = (unsigned char) deref_ext;
}

/* A member's address from its struct's: the offset added, as bytes, and the
 * member's type put on it. A member that is itself an array or a struct is
 * an object like any other, reached the same way. */
void vmember(int offset, Type type, int ext)
{
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
    if (type_is_struct(to)) {
        vcopy_struct();

        return;
    }

    vconvert(to);

    if (type_wide(to)) {
        int slot = long_scratch();
        int i;

        /* The value first, because building it may want HL, and the address
         * afterwards, which is what is left underneath it. */
        materialise_long(slot, to);
        vdrop();
        force_into(vsp - 1, R_HL);
        need_disp(slot);
        need_disp(slot + ACC_LONG_SIZE - 1);
        for (i = 0; i < ACC_LONG_SIZE; i++) {
            ld_a_ix(slot + i);
            ld_hl_a();
            if (i < ACC_LONG_SIZE - 1)
                inc_hl();
        }
        vdrop();
        vpush(VAL_LOCAL, to, slot);

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
    }
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

    if (type_wide(top->type)) {
        Type type = top->type;
        int slot = long_scratch();

        need_disp(slot);
        need_disp(slot + ACC_LONG_SIZE - 1);
        materialise_long(slot, type);
        vdrop();
        vpush(VAL_LOCAL, type, slot);

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
    vapply(op, type_size(type) < ACC_INT_SIZE ? type : 0);
}

/* ++x and --x on a local: x changed, and the answer is its new value. */
void vprefix_local(int offset, Type type, int ext, int op)
{
    vpush_local(offset, type);
    vset_ext(ext);
    vstep(op, type);
    vstore_local(offset, type);
}

/* x++ and x-- on a local: x changed, and the answer is its old value. */
void vpostfix_local(int offset, Type type, int ext, int op)
{
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
    Type target = type_deref(vtype());

    vdup();
    vderef();
    vstep(op, target);
    vstore_indirect();
}

void vpostfix_indirect(int op)
{
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
static Type cond_type(Type a, int a_null, Type b, int b_null)
{
    if (type_pointer(a) || type_pointer(b)) {
        if (a == b)
            return a;
        if (type_pointer(a) && b_null)
            return a;
        if (type_pointer(b) && a_null)
            return b;
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
int gen_cond_begin(int *slot)
{
    *slot = long_scratch();
    need_disp(*slot);
    need_disp(*slot + ACC_LONG_SIZE - 1);
    save_regs_below(1);

    return jump_on_truth(0);
}

/* After the middle operand: park it as the type it is, and jump forward to a
 * stub that does not exist yet. What the answer's type is depends on the
 * third operand, which has not been parsed, so converting now would be
 * guessing. */
int gen_cond_middle(int slot, Type *middle, int *middle_ext, int *middle_null)
{
    Value *top = vsp - 1;

    *middle = top->type;
    *middle_ext = top->ext;
    *middle_null = (top->kind == VAL_CONST && top->val == 0);
    cond_park(slot);

    return jump_op(JP_ANY);
}

/* After the third operand, when both types are known. The third converts
 * and parks where it is; then the stub the middle jumped to is written, and
 * it finds the middle's value exactly where it was left -- nothing between
 * the jump and here ran on that path -- converts it the same way, and parks
 * it in the same place. */
void gen_cond_end(int to_stub, int slot, Type middle, int middle_ext,
                  int middle_null)
{
    Value *top = vsp - 1;
    int third_null = (top->kind == VAL_CONST && top->val == 0);
    Type result = cond_type(middle, middle_null, top->type, third_null);
    int ext = third_null ? middle_ext : top->ext;   /* the side that is not 0 */
    int done;

    vconvert(result);
    cond_park(slot);
    done = jump_op(JP_ANY);

    patch_to_here(to_stub);
    if (type_wide(middle))
        vpush(VAL_LOCAL, middle, slot);
    else
        vpush(VAL_REG, type_promote(middle), R_HL);
    vconvert(result);
    cond_park(slot);

    patch_to_here(done);
    if (type_wide(result))
        vpush(VAL_LOCAL, result, slot);
    else
        vpush(VAL_REG, result, R_HL);
    (vsp - 1)->ext = (unsigned char) ext;
}

/* ------------------------------------------------------------------ */
/* taking code back                                                    */

void gen_mark(GenMark *m)
{
    m->at = out_here();
    m->nfixups = nfixups;
    m->nrt_fixups = nrt_fixups;
    m->narray_patches = narray_patches;
    m->spill_used = spill_used;
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
    out_rewind(m->at);
    nfixups = m->nfixups;
    nrt_fixups = m->nrt_fixups;
    narray_patches = m->narray_patches;
    spill_used = m->spill_used;
    vtop = m->vtop;
    vsp = vstack + vtop;
    rt_any_used = m->rt_any_used;
    if (m->saved) {
        memcpy(vstack, m->saved, (size_t) vtop * sizeof *m->saved);
        free(m->saved);
    }
}

void vcast(Type to, int ext)
{
    if (to == TY_VOID) {
        vdrop();
        vpush(VAL_VOID, TY_VOID, 0);

        return;
    }
    if (vsp[-1].kind == VAL_VOID)
        void_used();
    vconvert(to);
    if (type_pointer(to))
        vsp[-1].ext = (unsigned char) ext;
}
