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
        && !(val_const(top->kind) && top->val == 0)) {
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
        need_disp(slot);
        need_disp(slot + type_wide_bytes(to) - 1);
        materialise_long(slot, to);
        vdrop();
        vpush(VAL_LOCAL, to, slot);

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
        if (top->kind == VAL_ADDR)
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
    uint64_t bits = v->kind == VAL_WIDE ? wide_value(v)
                  : type_unsigned(v->type) ? (uint64_t) (uint32_t) v->val
                  : (uint64_t) (int64_t) v->val;
    int from_float = type_float(v->type), to_float = type_float(to);

    if (to_float && !from_float) {
        int negative = !type_unsigned(v->type)
                       && wide_signed(bits, type_wide_bytes(v->type)) < 0;
        uint64_t magnitude = negative
                             ? (uint64_t) -wide_signed(bits,
                                                       type_wide_bytes(v->type))
                             : bits;

        return float_from_int((uint32_t) magnitude, negative);
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

            need_disp(off);
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

            need_disp(off);
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
    if (val_const(val->kind)) {
        if (val->kind == VAL_ADDR)
            out_reloc(out_here() + 1);
        ld_rr_imm(reg, val->val);
    } else if (val->kind == VAL_WIDE) {
        ld_rr_imm(reg, (int) (wide_value(val) & 0xffffff));
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
    } else if (val_const(target->kind)) {
        if (target->kind == VAL_ADDR)
            out_reloc(out_here() + 1);
        ld_rr_imm(want, target->val);
    } else if (target->kind == VAL_WIDE) {
        ld_rr_imm(want, (int) (wide_value(target) & 0xffffff));
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
 * a linker must not do is write one of them down. Said at run time -- the
 * address into a variable first, the arithmetic after -- they all still
 * work, because then the address in the image is a whole one. */
static int fold_addr(int op, const Value *lhs, const Value *rhs)
{
    int left = lhs->kind == VAL_ADDR, right = rhs->kind == VAL_ADDR;

    if (!left && !right)
        return 0;
    if (op == TK_PLUS && left != right)
        return 1;
    if (op == TK_MINUS && left)
        return !right;
    no_addr_arithmetic();

    return 0;
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
    if (val_const(lhs->kind) && val_const(rhs->kind)
        && const_fold(op, lhs->val, rhs->val, &folded)) {
        Type folded_type = either_unsigned(lhs, rhs) ? TY_UINT : TY_INT;
        /* The kinds are in hand from the test above, and neither is an
         * address in almost every fold a program does, so the question is
         * asked here and the answer worked out elsewhere. */
        int addr = (lhs->kind == VAL_ADDR || rhs->kind == VAL_ADDR)
                   && fold_addr(op, lhs, rhs);

        vdrop();
        vdrop();
        vpush_const(folded, folded_type);
        if (addr)
            (vsp - 1)->kind = VAL_ADDR;

        return;
    }

    /* Adding or subtracting nothing is nothing. Worth the two lines: it is
     * what makes `p + 0` and the zero cases of generated code free. */
    if (val_const(rhs->kind) && rhs->val == 0
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
    if (type_float(type) != type_float((vsp - 1)->type) || type == TY_BOOL)
        vconvert(type);         /* a _Bool from all of a long, not its low
                                 * bytes, which the narrow store takes */

    if (type_wide(type)) {
        need_disp(offset);
        need_disp(offset + type_wide_bytes(type) - 1);
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

        need_disp(slot + ACC_LONG_SIZE - 1);
        ld_a_ix(slot + ACC_LONG_SIZE - 1);
        out_byte2(0xee, 0x80);          /* xor a, 0x80 */
        ld_ix_a(slot + ACC_LONG_SIZE - 1);
        vpush(VAL_LOCAL, top->type, slot);

        return;
    }

    if (type_wide(top->type)) {
        vunary_long(type_eight(top->type) ? RT_LLNEG : RT_LNEG, top->type);

        return;
    }

    if (val_const(top->kind)) {
        if (top->kind == VAL_ADDR)
            no_addr_arithmetic();
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

static void fixup_add(int fn, int at)
{
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
    if (val->kind == VAL_ADDR)
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
        if (!val_const(rhs->kind) || rhs->val < 0 || rhs->val > 8)
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

    if (val_const(lhs->kind) && val_const(rhs->kind)
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

    out_byte3(0xed, lea_code[reg], disp);
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

    for (i = 0; i < n; i++) {
        out_byte3(0xdd, 0x7e, from + i);        /* ld a, (ix+d) */
        out_byte3(0xdd, 0x77, to + i);          /* ld (ix+d), a */
    }
}

/* The bytes of a slot from `from` to `to` set to A: the extension of a
 * value that is narrower than the slot. */
static void fill_from_a(int disp, int from, int to)
{
    need_disp(disp + to - 1);
    for (; from < to; from++)
        ld_ix_a(disp + from);
}

/* Write an int-wide value, already in HL, into a slot of n bytes: three
 * bytes and then the ones the sign or the zero extension calls for. */
static void store_int_as_long(int disp, int is_unsigned, int n)
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

        need_disp(slot);
        need_disp(slot + type_wide_bytes(top->type) - 1);
        lea_rr_ix(R_HL, slot);
        if (eight)
            rt_call(unsign ? RT_ULLTOF : RT_LLTOF);
        else
            rt_call(unsign ? RT_ULTOF : RT_LTOF);
        vpush(VAL_LOCAL, TY_FLOAT, slot);

        return;
    }

    force_into(top, R_HL);

    slot = long_scratch(TY_FLOAT);
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
     * do not fit in a register -- but materialise_long is what says so. A
     * long long's slot is eight bytes, the float in the first four. */
    slot = type_eight(to) ? long_scratch(to) : long_scratch(TY_FLOAT);
    materialise_long(slot, TY_FLOAT);
    vdrop();

    need_disp(slot);
    need_disp(slot + (type_wide(to) ? type_wide_bytes(to) : ACC_LONG_SIZE) - 1);
    lea_rr_ix(R_HL, slot);

    /* A long stays in the frame, where the routine rewrites it in place. */
    if (type_wide(to)) {
        rt_call(type_eight(to) ? RT_FTOLL : RT_FTOL);
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

/* An address moves with the image; the floating-point value made from one
 * cannot, because its bytes are no longer an address for a relocation to
 * name. Only the two paths that reach a float from an integer ask, so the
 * conversion every argument of every call goes through does not. */
static void no_float_address(const Value *from)
{
    if (from->kind == VAL_ADDR)
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
    if (val_const(from->kind) && from->val == 0)
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
    if (top->kind == VAL_ADDR) {
        int reg = force_reg(top);
        int i;

        need_disp(disp);
        need_disp(disp + n - 1);
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
 * store, a byte at a time. */
static void wide_bytes_at(int disp, uint64_t bits, int n)
{
    int i;

    need_disp(disp);
    need_disp(disp + n - 1);
    for (i = 0; i < n; i++) {
        out_byte(0x3e);                         /* ld a, n */
        out_byte((int) (bits >> (i * 8)) & 0xff);
        ld_ix_a(disp + i);
    }
}

/* And a slot of its own for it, which is what a wide constant becomes when
 * something needs it where a wide value lives. */
static void wide_to_slot(uint64_t bits, Type type)
{
    int slot = spill_slot_of(type_wide_bytes(type));

    wide_bytes_at(slot, bits, type_wide_bytes(type));
    vpush(VAL_LOCAL, type, slot);
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

    need_disp(slot);
    need_disp(slot + type_wide_bytes(type) - 1);
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

    need_disp(left);
    need_disp(right);
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

    need_disp(left);
    need_disp(right);

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

static int jump_op(int op)
{
    int hole;

    out_opcode24(op, 0);
    hole = out_here() - ACC_INT_SIZE;
    out_reloc(hole);

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
    out_reloc(out_here() + 1);
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
void gen_switch_case(long value, uint32_t high, Type type, int target,
                     int slot)
{
    /* A long long compares its top five bytes from the frame, one at a time,
     * and the low three in HL as anything else does. Each miss jumps past
     * the rest of the case: the groups after it, seven bytes each, and the
     * twelve of the tail. */
    if (type_eight(type)) {
        int k;

        need_disp(slot + 7);
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
static int rt_any_used;                 /* 1 for the first part, 2 for all */

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
    /* The long long routines are past RT_SPLIT, and only a program that
     * calls one of them carries them. */
    if (rt_entry[which] >= RT_SPLIT)
        rt_any_used = 2;
    else if (!rt_any_used)
        rt_any_used = 1;
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

    len = rt_any_used == 2 ? (int) sizeof rt_code : RT_SPLIT;
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
 * Where any of it goes is not known until the whole image has been written,
 * so each one is a symbol without an address until then -- which is what a
 * variable another file defines already was, and rides the same fixups. What
 * that costs is the folding: `a[3]` is a load and an add where a variable
 * whose address was known as it was compiled is one load.
 *
 * Only what is at file scope, for now. A block's `static` still takes its
 * zeros in the file, because it is a local symbol that is gone by the time
 * the addresses are handed out, and reaching it needs a relocation against
 * the start of the bss rather than against a symbol of its own. */
typedef struct {
    int sym, at;
} BssSym;

static BssSym *bss_syms;
static int     nbss_syms, bss_syms_cap;
static int     bss_len;
static int     bss_init_hole = -1;      /* the stub's call to the clearing */

/* Room in the bss, and where in it. The linker asks for a whole object's
 * worth at once and hands out the pieces itself. */
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

        return;
    }

    base = out_here() + (bss_len == 1 ? BSS_INIT_ONE : BSS_INIT_LEN);
    if (base - out_base + bss_len > ACC_RAM_BYTES)
        acc_error("the program and what it leaves at zero come to %d bytes, "
                  "and the Agon has %d for both",
                  base - out_base + bss_len, ACC_RAM_BYTES);

    out_byte(0x21);                             /* ld hl, base */
    out_reloc(out_here());
    out_word24(base);
    out_byte2(0x36, 0x00);                      /* ld (hl), 0 */
    if (bss_len > 1) {
        out_byte(0x11);                         /* ld de, base + 1 */
        out_reloc(out_here());
        out_word24(base + 1);
        out_byte(0x01);                         /* ld bc, bss_len - 1 */
        out_word24(bss_len - 1);
        out_byte2(0xed, 0xb0);                  /* ldir */
    }
    out_byte(0xc9);                             /* ret */

    for (i = 0; i < nbss_syms; i++) {
        sym_at(bss_syms[i].sym)->val = base + bss_syms[i].at;
        sym_set_flags(bss_syms[i].sym, SYMF_DEFINED);
    }
}

/* A symbol the fixups are waiting on that nothing here has given an address
 * to. A function has one once its body has been read -- asked of the flag
 * and not of the address, because an object puts its first function at
 * offset zero. A variable has one once room has been reserved for it, which
 * a declaration that only says extern does not do: that leaves -1 behind. */
static int no_address(int sym)
{
    return sym_at(sym)->kind == SYM_FUNC
           ? !(sym_flags(sym) & SYMF_DEFINED)
           : sym_at(sym)->val < 0;
}

void gen_finish(void)
{
    int i;

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
        bss_emit();
    }

    for (i = 0; i < nfixups; i++) {
        Sym *fn = sym_at(fixups[i].fn);

        if (no_address(fixups[i].fn)) {
            if (gen_objects) {
                extern_add(fixups[i].at, fixups[i].fn);
                continue;
            }
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
    int base;
    int i;

    /* Clearing what starts at zero comes first, and is a routine emitted at
     * the end once there is an address and a length for it. The call is
     * here whether there turns out to be anything to clear or not: four
     * bytes and a `ret`, against working out how to not have made the call. */
    out_reloc(out_here() + 1);
    out_opcode24(0xcd, 0);                       /* call the clearing */
    bss_init_hole = out_here() - ACC_INT_SIZE;

    base = out_here();
    for (i = 0; i < n; i++)
        out_byte(stub[i]);

    /* The call to main is the first instruction in either version. */
    fixup_add(m, base + 1);

    if (!report_by_exit)
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
    need_disp(slot);
    ld_ix_rr(slot, R_HL);               /* and that is where the array is */
}

void gen_stack_mark(int slot)
{
    evict_reg(R_HL);
    ld_rr_imm(R_HL, 0);
    out_byte(0x39);                     /* add hl, sp */
    need_disp(slot);
    ld_ix_rr(slot, R_HL);
}

void gen_stack_back(int slot)
{
    evict_reg(R_HL);
    need_disp(slot);
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
             * loads. An eight-byte one is HL, DE and BC from the bottom up,
             * BC's upper byte reading one past the slot, which the caller
             * does not look at. */
            int at;

            vconvert(return_type);
            wide_needs_slot();
            at = (vsp - 1)->val;
            need_disp(at);
            if (type_eight(return_type)) {
                need_disp(at + 2 * ACC_INT_SIZE);
                ld_rr_ix(R_HL, at);
                ld_rr_ix(R_DE, at + ACC_INT_SIZE);
                ld_rr_ix(R_BC, at + 2 * ACC_INT_SIZE);
            } else {
                need_disp(at + ACC_LONG_SIZE - 1);
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

void gen_call(int fn, int nargs, int params_first, int nparams)
{
    Callee callee;
    const Sym *f = sym_at(fn);

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

            need_disp(slot);
            need_disp(slot + 2 * ACC_INT_SIZE);
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
        need_disp(slot);
        need_disp(slot + 7);
        ld_ix_rr(slot, R_HL);
        ld_ix_rr(slot + ACC_INT_SIZE, R_DE);
        out_byte(0x79);                         /* ld a, c */
        ld_ix_a(slot + 2 * ACC_INT_SIZE);
        out_byte(0x78);                         /* ld a, b */
        ld_ix_a(slot + 7);
        vpush(VAL_LOCAL, callee->type, slot);
        (vsp - 1)->ext = (unsigned char) callee->ext;

        return;
    }

    for (i = 0; i < argslots; i++)
        pop_rr(type_wide(callee->type) ? R_BC : R_DE);

    if (type_wide(callee->type)) {
        /* HL with the high byte in E; put it where every long lives. */
        int slot = spill_slot_of(ACC_LONG_SIZE);

        need_disp(slot);
        need_disp(slot + ACC_LONG_SIZE - 1);
        ld_ix_rr(slot, R_HL);
        out_byte(0x7b);                          /* ld a, e */
        ld_ix_a(slot + ACC_INT_SIZE);
        vpush(VAL_LOCAL, callee->type, slot);
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
        out_word24(fp->val);
    } else if (fp->kind == VAL_LOCAL) {
        need_disp(fp->val);
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
    if (type_pointer(left) && type_pointer(right) && left != right)
        acc_error_at(tok_line, "these are pointers to different types");
}

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

        need_disp(slot);
        need_disp(slot + n - 1);
        for (i = 0; i < n; i++) {
            ld_a_hl();
            ld_ix_a(slot + i);
            if (i < n - 1)
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
        need_disp(slot);
        need_disp(slot + n - 1);
        for (i = 0; i < n; i++) {
            ld_a_ix(slot + i);
            ld_hl_a();
            if (i < n - 1)
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
        (vsp - 1)->quals = kept.quals;
    }
}

/* A copy of the value `depth` below the top, pushed: vdup, from further
 * down. A register is moved to the frame first, for vdup's reason. */
static void vpick(int depth)
{
    Value *from = vsp - 1 - depth;

    if (from->kind == VAL_ACC || from->kind == VAL_REG) {
        int off = spill_slot();

        if (from->kind == VAL_ACC)
            force_reg(from);
        need_disp(off);
        ld_ix_rr(off, from->val);
        from->kind = VAL_LOCAL;
        from->val = off;
    }
    vpush(from->kind, from->type, from->val);
    (vsp - 1)->ext = from->ext;
}

/* The unit a bit-field is read from and written to: its bytes, as the
 * unsigned type that many bytes are. */
static Type bitfield_unit(const BitField *bf)
{
    static const Type units[5] = { 0, TY_UCHAR, TY_USHORT, TY_UINT, TY_ULONG };

    return units[bf->bytes];
}

/* A bit-field's value, from its address on the stack: its unit read, shifted
 * down and masked -- and, for a signed one, the top bit it has spread up
 * through the rest. Narrower than an int, it is an int, as C promotes it. */
static void bitfield_read(void)
{
    const BitField *bf = bitfield_at((vsp - 1)->bits);
    Type unit = bitfield_unit(bf);
    unsigned long mask = bf->width >= 32 ? 0xffffffffUL
                                         : (1UL << bf->width) - 1;

    (vsp - 1)->bits = 0;
    (vsp - 1)->type = type_ptr_to(unit);
    vderef();
    if (bf->pos) {
        vpush_const(bf->pos, TY_INT);
        vapply(TK_SHR, 0);
    }
    if (bf->pos + bf->width < bf->bytes * 8) {
        if (unit == TY_ULONG)
            vpush_const_long((long) mask, TY_ULONG);
        else
            vpush_const((int) mask, TY_UINT);
        vapply(TK_AMP, 0);
    }
    if (unit == TY_ULONG && bf->width <= ACC_INT_SIZE * 8)
        vconvert(TY_UINT);
    if (bf->is_signed && bf->width < (unit == TY_ULONG ? 32 : ACC_INT_SIZE * 8)) {
        /* (v ^ sign) - sign: the sign bit's weight made negative. */
        Type t = type_wide(vtype()) ? TY_LONG : TY_INT;
        long sign = 1L << (bf->width - 1);

        if (t == TY_LONG) {
            vpush_const_long(sign, TY_LONG);
            vapply(TK_CARET, 0);
            vpush_const_long(sign, TY_LONG);
        } else {
            vpush_const((int) sign, TY_INT);
            vapply(TK_CARET, 0);
            vpush_const((int) sign, TY_INT);
        }
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
    int wide = unit == TY_ULONG;
    unsigned long mask = bf->width >= 32 ? 0xffffffffUL
                                         : (1UL << bf->width) - 1;
    unsigned long keep = ~(mask << bf->pos)
                         & (bf->bytes >= 4 ? 0xffffffffUL
                                           : (1UL << bf->bytes * 8) - 1);

    /* The value, cut to the width: [addr, v]. A _Bool field is 0 or 1
     * first, as any _Bool is. */
    if (type_deref((vsp - 2)->type) == TY_BOOL)
        vconvert(TY_BOOL);
    vconvert(wide ? TY_ULONG : TY_UINT);
    if (wide)
        vpush_const_long((long) mask, TY_ULONG);
    else
        vpush_const((int) mask, TY_UINT);
    vapply(TK_AMP, 0);

    /* Two copies of the address, to read the unit through and to write it
     * back through, and the unit read with the field's bits cleared:
     * [addr, v, addr, old]. */
    vpick(1);
    (vsp - 1)->bits = 0;
    (vsp - 1)->type = type_ptr_to(unit);
    vdup();
    vderef();
    if (wide)
        vpush_const_long((long) keep, TY_ULONG);
    else
        vpush_const((int) keep, TY_UINT);
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
    if (bf->is_signed && bf->width < (wide ? 32 : ACC_INT_SIZE * 8)) {
        long sign = 1L << (bf->width - 1);

        if (wide) {
            vpush_const_long(sign, TY_LONG);
            vapply(TK_CARET, 0);
            vpush_const_long(sign, TY_LONG);
        } else {
            vpush_const((int) sign, TY_INT);
            vapply(TK_CARET, 0);
            vpush_const((int) sign, TY_INT);
        }
        vapply(TK_MINUS, 0);
    }
    if (wide && bf->width <= ACC_INT_SIZE * 8)
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

        need_disp(slot);
        need_disp(slot + type_wide_bytes(type) - 1);
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
    vapply(op, type_narrow(type));
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
    *slot = long_scratch(TY_LONG);
    need_disp(*slot);
    need_disp(*slot + ACC_LONG_SIZE - 1);
    save_regs_below(1);

    return jump_on_truth(0);
}

/* After the middle operand: park it as the type it is, and jump forward to a
 * stub that does not exist yet. What the answer's type is depends on the
 * third operand, which has not been parsed, so converting now would be
 * guessing. */
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
void gen_cond_end(int to_stub, int slot, Type middle, int middle_ext,
                  int middle_null)
{
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
        vpush(VAL_LOCAL, middle, slot);
    else
        vpush(VAL_REG, type_promote(middle), R_HL);
    vconvert(result);
    cond_park(park);

    patch_to_here(done);
    if (type_wide(result))
        vpush(VAL_LOCAL, result, park);
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
    m->narray_patches = narray_patches;
    m->spill_used = spill_used;
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
    out_rewind(m->at);
    nfixups = m->nfixups;
    nrt_fixups = m->nrt_fixups;
    narray_patches = m->narray_patches;
    spill_used = m->spill_used;
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
