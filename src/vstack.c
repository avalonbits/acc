/*
 * The value stack and the registers.
 *
 * Values are not turned into instructions when they are parsed, only when
 * something needs them in a register. A constant stays a number, a local
 * stays a frame offset, and `x + 1` loads x once and adds an immediate
 * rather than loading both and adding registers. This is where a value
 * waits, and where it is put into a register when it has to be.
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

static int  reg_owner(int reg, const Value *except);

/* Narrow what is in HL to `to`, then widen it back, which is what a C
 * conversion to a narrow type leaves behind. */
static void convert_in_hl(Type to)
{
    if (type_size(to) >= ACC_INT_SIZE)
        return;

    if (type_size(to) == 1) {
        int from;

        ld_a_l();
        from = out_here();
        if (type_unsigned(to))
            fill_hl_with_zero();
        else
            fill_hl_with_sign_of_a();
        ld_l_a();
        widen_made(from, to);           /* A has the byte: see branch.c */

        return;
    }

    /* The low byte is kept in IY, the backend's own scratch, rather than
     * in E: DE may be holding a value, and this would have cost it its low
     * byte. */
    iy_save();
    out_byte3(0xe5, 0xfd, 0xe1);        /* push hl; pop iy */
    ld_a_h();
    if (type_unsigned(to))
        fill_hl_with_zero();
    else
        fill_hl_with_sign_of_a();
    ld_h_a();
    out_byte3(0xfd, 0x7d, 0x6f);        /* ld a, iyl; ld l, a */
    iy_restore();
}

/* The type this function was declared to return, so that `char f()` giving
 * back 300 gives back 44 as C says it must. */
Type return_type = TY_INT;
int  return_ext;         /* the struct it returns, when it does */
/* ------------------------------------------------------------------ */
/* the value stack                                                     */

Value vstack[VSTACK_MAX];
/* Never negative, and compared as if unsigned -- `vtop == 0`, `(unsigned)
 * vtop < 2` -- because a signed compare is a helper call on this target, and
 * these are in front of every value pushed and popped. test/helpers.sh holds
 * the hot ones to it. */
int   vtop;               /* number of live entries */

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
Value *vsp = vstack;

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
void check_reg_free(int reg)
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
int locals_size;          /* the declared locals */
int spill_used;           /* the scratch in use right now */

PendingSpill spill_pending[SPILL_PENDING];
unsigned char nspill_pending;     /* a byte: vpush tests it */
int spill_peak;           /* the most it ever held */
int spill_locked;         /* held by something not on the value stack */
int frame_patch;          /* where the prologue's frame size is written */
int frame_call;           /* and its call to acc_rt_frameset, among rt_fixups */

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
int arrays_size;          /* bytes of arrays sized so far */
int *array_end;           /* by array number: its end in the area */
int narrays, array_end_cap;

ArrayPatch *array_patches;
int narray_patches, array_patches_cap;

int frame_size(void)
{
    return locals_size + spill_peak + arrays_size;
}

void vcheck(void)
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

void vset_width(Value *v, int width)
{
    v->quals = (unsigned char) ((v->quals & ~(VQ_BYTE | VQ_WORD))
                                | (width == 1 ? VQ_BYTE | VQ_WORD
                                   : width == 2 ? VQ_WORD : 0));
}

/* The bits vwidth reads, for an unsigned narrow value being widened into a
 * register: the load fills with zeros above it. Asked where a value is
 * loaded, which is what the time goes on, so it is a byte test and no call. */
#define NARROW_QUALS(t) (!type_unsigned(t) ? 0 \
                         : type_size(t) == 1 ? VQ_BYTE | VQ_WORD : VQ_WORD)

void vpush_const(int val, Type type)
{
    vpush(VAL_CONST, type, val);
}

#ifdef OPT_ACC
/* How many values are on the stack, for inline.c, which reads it without
 * changing anything -- so not in gen.h, whose calls the log keeps. */
int gen_stack_depth(void)
{
    return vtop;
}
#endif

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
    if (offset == iy_local) {
        (vsp - 1)->kind = VAL_IY;
        (vsp - 1)->val = 0;
    }
}

/* ------------------------------------------------------------------ */
/* the local in IY                                                     */

/* A local that lives in IY for the whole of its function, rather than in its
 * frame slot: read with lea rr, iy+0 and written with ld iy or a push and a
 * pop, where every other local is ld rr, (ix+d) and ld (ix+d), rr at each
 * use -- which is most of the difference between acc's code and agondev's,
 * whose register allocator keeps the variables a loop walks in registers.
 *
 * One per function, and one that is declared register, so that its address
 * is never taken: the value has no bytes in memory to point at. It keeps
 * its frame slot, unused, so that its offset still names it; iy_local is
 * that offset. vpush_local and vstore_local turn the offset into IY, and
 * everything else reaches it through them.
 *
 * On the value stack it is VAL_IY, with a displacement: the value is IY
 * plus val. A small constant added to it only changes the displacement
 * (vbinop), so `p + 1` is no code, a store of it back is `inc iy`, the
 * old value of `p++` is IY - 1 without a copy, and reading or writing
 * through it is (iy+d) -- `*p++` is ld a, (iy-1).
 *
 * IY was the backend's own scratch, and still is where no local holds it:
 * in a function where one does, each of those uses saves it and puts it
 * back (iy_save), and so does every call, since the callee may use it. */
int iy_local;
int iy_any;

/* Whether a local of this type can: one register's worth, so a pointer or
 * an int, and nothing that is not a number to begin with. */
int gen_iy_can(Type type)
{
    return type_size(type) == ACC_INT_SIZE && !type_float(type)
           && !type_wide(type) && !type_is_struct(type);
}

/* A local as it is declared, with its qualifiers: whether it is the one. */
void gen_iy_claim(int offset, Type type, int quals)
{
    if (iy_local == 0 && ((quals & SQ_REGISTER) || iy_any)
        && gen_iy_can(type))
        iy_local = offset;
}

#ifdef OPT_ACC
/* The local at `offset` into IY: opt-acc's pre-scan choosing it
 * (prescan_claim), as a call through gen.h so that genlog.c sees it. */
void gen_iy_take(int offset)
{
    iy_local = offset;
}
#endif

/* A parameter as the list is read: its offset if it is the one, else 0. */
int gen_iy_pick(int offset, Type type, int quals)
{
    return ((quals & SQ_REGISTER) || iy_any) && gen_iy_can(type) ? offset : 0;
}

/* A parameter arrives in its slot above the frame pointer, so it is loaded
 * into IY once, as the body begins. */
void gen_iy_param(int offset)
{
    iy_local = offset;
    frame_byte(0x31, offset);           /* ld iy, (ix+d) */
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

    /* A narrow local going to a type as wide is read as that type: the
     * load is the whole of the conversion. And one just read through a
     * pointer is widened again as `to` wants, if it was not already. */
    if (top->kind == VAL_LOCAL && !top->bits
        && type_size(top->type) == type_size(to)) {
        top->type = to;
        force_reg(vsp - 1);             /* any register: an unsigned one
                                         * loads into DE or BC as it is */
        (vsp - 1)->type = type_promote(to);

        return;
    }
    if (widen_again(to))
        return;

    /* A value in a register that already fits: zero above the bytes `to`
     * keeps, and short of its sign bit if it has one -- or all of its
     * bytes, if it has none. There is nothing to convert. */
    if (top->kind == VAL_REG
        && (vwidth(top) < type_size(to)
            || (vwidth(top) == type_size(to) && type_unsigned(to)))) {
        top->type = type_promote(to);

        return;
    }

    force_into(vsp - 1, R_HL);
    convert_in_hl(to);
    top->type = type_promote(to);
    vset_width(top, type_unsigned(to) ? type_size(to) : 3);
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

#define WIDE_CONSTS 24
static uint64_t wide_consts[WIDE_CONSTS];
int      nwide_consts;

uint64_t wide_value(const Value *v)
{
    return wide_consts[v->val];
}

/* A wide value where the frame is what is wanted: a constant is written
 * into a slot of its own, and anything else is already in one. The paths
 * that push arguments and return a result read the slot itself, so this is
 * what they call before they do. */
void wide_needs_slot(void)
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
int wide_table_of(const Value *a, const Value *b)
{
    if (a->kind == VAL_WIDE)
        return a->val;
    if (b->kind == VAL_WIDE)
        return b->val;

    return -1;
}

/* Whether the top two values are both constants, which is what lets an
 * operator work its answer out rather than emit it. */
int vconst_pair(void)
{
    const Value *a = vsp - 2, *b = vsp - 1;

    return vtop >= 2
        && (val_const(a->kind) || a->kind == VAL_WIDE)
        && (val_const(b->kind) || b->kind == VAL_WIDE);
}

/* A constant of any kind as the bits `to` would hold: a narrow one widens by
 * its own sign, a long long narrows to a long's four bytes, and an integer
 * becoming a float is arithmetic, not a relabelling. */
uint64_t const_as(const Value *v, Type to)
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
int wide_push(uint64_t bits, Type type, int ext)
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
void save_regs_below(int n)
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

int reg_alloc(void)
{
    int reg;

    for (reg = 0; reg != NREGS; reg++)
        if (!reg_busy(reg))
            return reg;
    spill_one();
    for (reg = 0; reg != NREGS; reg++)
        if (!reg_busy(reg))
            return reg;
    acc_error("internal: no register after spilling");

    return 0;
}

/* A register that is free and is not `avoid`, spilling to make one if need
 * be. */
int reg_alloc_other(int avoid)
{
    int reg;

    for (reg = 0; reg != NREGS; reg++)
        if (reg != avoid && !reg_busy(reg))
            return reg;
    spill_one_other(avoid);
    for (reg = 0; reg != NREGS; reg++)
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
int force_reg(Value *val)
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

    if (val->kind == VAL_IYADDR)
        acc_error_at(tok_line, "internal: the address of the local in IY");
    if (val->kind == VAL_IY) {
        reg = reg_alloc();
        lea_rr_iy(reg, val->val);
        val->kind = VAL_REG;
        val->val = reg;

        return reg;
    }

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
        int from = out_here();

        reg = R_HL;
        if (type_unsigned(val->type))
            fill_hl_with_zero();
        else
            fill_hl_with_sign_of_a();
        ld_l_a();
        widen_made(from, val->type);    /* A has it still: see branch.c */
        val->quals |= NARROW_QUALS(val->type);
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
        if (type_size(val->type) < ACC_INT_SIZE) {
            load_narrow_into(reg, val->val, val->type);
            val->quals |= NARROW_QUALS(val->type);
        } else {
            ld_rr_ix(reg, val->val);
        }
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

    for (reg = 0; reg != NREGS; reg++)
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
void evict_reg(int reg)
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

void force_into(Value *target, int want)
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
        target->quals |= NARROW_QUALS(target->type);
        if (want != R_HL)
            mov_rr(want, R_HL);
        if (keep)
            pop_rr(R_HL);
    } else if (target->kind == VAL_REG) {
        if (target->val != want)
            mov_rr(want, target->val);
    } else if (target->kind == VAL_IYADDR) {
        acc_error_at(tok_line, "internal: the address of the local in IY");
    } else if (target->kind == VAL_IY) {
        lea_rr_iy(want, target->val);
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
        if (type_size(target->type) < ACC_INT_SIZE) {
            load_narrow_into(want, target->val, target->type);
            target->quals |= NARROW_QUALS(target->type);
        } else {
            ld_rr_ix(want, target->val);
        }
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


void gen_init(void)
{
#ifndef AGONDEV
    iy_any = getenv("ACC_IY_ANY") != NULL;      /* see test/iyany.sh */
#endif
    vtop = 0;
    vsp = vstack;
    rt_syms_init();
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
    logic_from = -1;
    widen_from = -1;
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
