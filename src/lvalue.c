/*
 * Addresses and what is at them: pointer arithmetic, taking an address and
 * following one, members, stores, bitfields, and ++ and --.
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

static void bitfield_read(void);
static void bitfield_write(void);

/* The value on top into IY, where the local that lives there is: see
 * iy_local. The value stays on the stack as the assignment's answer.
 *
 * Another read of the local still waiting on the stack would read what is
 * written here instead of what was there, so it is taken first -- `x++`
 * reads it that way already, and nothing else is ordered between its read
 * and a write. */
__attribute__((noinline)) static void vstore_iy(void)
{
    Value *top = vsp - 1, *v;

    if (top->kind == VAL_IY && top->val == 0)
        return;
    for (v = vstack; v < top; v++)
        if (v->kind == VAL_IY)
            force_reg(v);
    if (top->kind == VAL_IY) {
        /* The local and a step: a step, and the answer is what it now is.
         * inc iy and dec iy for one, lea iy, iy+d for the rest. */
        if (top->val == 1 || top->val == -1)
            out_iy(top->val == 1 ? 0x23 : 0x2b);
        else
            out_byte3(0xed, 0x33, top->val);
        top->val = 0;
    } else if (top->kind == VAL_LOCAL && !top->bits
               && type_size(top->type) == ACC_INT_SIZE) {
        frame_byte(0x31, top->val);             /* ld iy, (ix+d) */
    } else {
        push_rr(force_reg(top));
        out_iy(0xe1);                           /* pop iy */
    }
}

void vstore_local(int offset, Type type)
{
    gen_effects++;
    int reg;

    /* An assignment converts the value to the type of the object, and
     * between a float and an integer that is arithmetic rather than a
     * relabelling. vconvert is where it lives; this used to refuse instead,
     * which is why a float could be stored and read back but never made from
     * anything. The local in IY needs it as much as one in the frame. */
    if (type_float(type) != type_float((vsp - 1)->type) || type == TY_BOOL)
        vconvert(type);         /* a _Bool from all of a long, not its low
                                 * bytes, which the narrow store takes */

    if (offset == iy_local) {
        vstore_iy();

        return;
    }

    if (type_wide(type)) {
        if (long_into(offset, type))
            return;
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
        /* A byte just read and widened is still in A: stored from there,
         * and widened again only for the value, as below. */
        if (type_size(type) == 1 && store_byte_widened(offset, type))
            return;

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
        conversion_epoch = out_rewinds;

        return;
    }

    reg = force_reg(vsp - 1);
    ld_ix_rr(offset, reg);
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
void vcmp_pointer_check(Type left, Type right)
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

void vbinop_pointer(int op, Type left, Type right)
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
    /* The local in IY has no address, but the parser asks for one where a
     * name is an object rather than a value -- `(x) = v`, `sizeof x` -- and
     * only ever reads or writes through it: vderef turns this back into the
     * local, and vstore_indirect writes the local. Loading it as a number
     * is what `&x` would do, which `register` rules out. */
    if (offset == iy_local) {
        vpush(VAL_IYADDR, type_ptr_to(type), 0);

        return;
    }

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
    if (top->kind == VAL_IYADDR) {
        top->kind = VAL_IY;
        top->type = to;

        return;
    }

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

    /* Through the local in IY: read at (iy+d), with no copy of the pointer
     * made first -- ld a, (iy+d) for a byte and ld hl, (iy+d) for three. */
    if (top->kind == VAL_IY && !type_wide(to) && type_size(to) != 2) {
        evict_reg(R_HL);
        out_iy_d(type_size(to) == 1 ? 0x7e : 0x27, top->val);
        goto read;
    }

    force_into(top, R_HL);

    if (type_wide(to)) {
        int n = type_wide_bytes(to);
        int slot = long_scratch(to);

        wide_through_hl(slot, n, 0);
        vdrop();
        vpush_scratch(to, slot);

        return;
    }

    /* The address just loaded as a constant: read through it directly, by
     * turning the load into ld hl, (nn) or ld a, (nn) -- six bytes, or
     * five, into four. Not where something jumps in after the load, which
     * would skip the read. */
    if (type_size(to) != 2 && imm_hl_end == out_here()
        && imm_hl_epoch == out_rewinds && join_at != out_here()) {
        out_img[out_here() - ACC_INT_SIZE - 1 - out_base] =
            type_size(to) == 1 ? 0x3a : 0x2a;   /* ld a, (nn); ld hl, (nn) */
        imm_hl_end = -1;
        gaddr_end = -1;         /* HL holds what is there, not the address */
        if (type_size(to) == 1)
            widen_loaded(to);
    } else if (type_size(to) == ACC_INT_SIZE) {
        ld_hl_ind_hl();
    } else if (type_size(to) == 1) {
        ld_a_hl();
read:
        if (type_size(to) == 1)
            widen_loaded(to);
    } else {
        /* Two bytes, read through IY: loading either into H or into L would
         * overwrite the pointer before the other had been read, and keeping
         * the low one in E, as this once did, overwrote whatever the
         * allocator was holding in DE. IY is the backend's own scratch. */
        iy_save();
        out_byte3(0xe5, 0xfd, 0xe1);    /* push hl; pop iy */
        out_byte3(0xfd, 0x7e, 0x01);    /* ld a, (iy+1) */
        widen_loaded(to);
        iy_restore();
    }

    vdrop();
    vpush_reg(R_HL);
    (vsp - 1)->type = type_promote(to);
    if (type_unsigned(to) && type_size(to) < ACC_INT_SIZE && !type_pointer(to))
        vset_width(vsp - 1, type_size(to));
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

/* The value is the answer; the address has done its work. */
static void vstore_leave_value(void)
{
    Value kept = *(vsp - 1);

    vdrop();
    vdrop();
    vpush(kept.kind, kept.type, kept.val);
    (vsp - 1)->ext = kept.ext;
    (vsp - 1)->quals = kept.quals;
}

/* The byte on top written through the address under it, from A, where it
 * is a byte already and the address is in a pair or in the frame: the two
 * left as the byte in A. Whether it was. */
static int store_byte_through(Type to)
{
    Value *val = vsp - 1, *at = vsp - 2;
    int local = val->kind == VAL_LOCAL && type_size(val->type) == 1, reg;

    if (val->bits || at->bits || (at->kind != VAL_REG && at->kind != VAL_LOCAL)
        || (!local && val->kind != VAL_ACC && !widen_undo(val)))
        return 0;
    val->kind = VAL_ACC;                /* HL free for the address */
    reg = force_reg(at);                /* never A: a load or nothing */
    if (local)
        ld_a_ix(val->val);
    ld_ind_a(reg);
    val->type = to;
    vstore_leave_value();

    return 1;
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
    if ((vsp - 2)->kind == VAL_IYADDR) {
        vstore_iy();
        vstore_leave_value();

        return;
    }

    if (type_wide(to)) {
        int n = type_wide_bytes(to);
        int slot;

        /* A value already in a slot of its own at this width is stored
         * from there, and is the answer as it was: copying it into scratch
         * first was the same bytes moved twice. */
        if ((vsp - 1)->kind == VAL_LOCAL && !(vsp - 1)->bits
            && type_wide((vsp - 1)->type)
            && type_wide_bytes((vsp - 1)->type) == n) {
            Value kept = *(vsp - 1);

            vdrop();
            force_into(vsp - 1, R_HL);
            wide_through_hl(kept.val, n, 1);
            vdrop();
            vpush(kept.kind, kept.type, kept.val);
            (vsp - 1)->ext = kept.ext;
            (vsp - 1)->quals = kept.quals;

            return;
        }

        /* The value first, because building it may want HL, and the address
         * afterwards, which is what is left underneath it. */
        slot = long_scratch(to);
        materialise_long(slot, to);
        vdrop();
        force_into(vsp - 1, R_HL);
        wide_through_hl(slot, n, 1);
        vdrop();
        vpush_scratch(to, slot);

        return;
    }

    /* Through the local in IY: written at (iy+d), from A for a byte or from
     * any register for three, and the pointer is not copied at all. */
    if ((vsp - 2)->kind == VAL_IY && type_size(to) != 2) {
        Value *val = vsp - 1;
        int d = (vsp - 2)->val;

        if (val->kind == VAL_ACC && type_size(to) == 1) {
            out_iy_d(0x77, d);                          /* ld (iy+d), a */
        } else {
            static const unsigned char low_of[NREGS] = { 0x75, 0x73, 0x71 };
            int reg = force_reg(val);

            /* ld (iy+d), rr for three bytes, as ld (ix+d), rr has it, and
             * ld (iy+d), l, e or c for one. */
            out_iy_d(type_size(to) == ACC_INT_SIZE ? 0x0f + reg_code[reg]
                                                   : low_of[reg], d);
        }
        vstore_leave_value();

        return;
    }

    /* To an address known as a constant -- a global of this file -- the
     * store names it: ld (nn), hl, de or bc for three bytes, ld (nn), a
     * for one, where it was the address loaded into HL and a store through
     * it. The operand is relocated, or pointed at the bss, as a load of the
     * address would have been. */
    if (val_pending((vsp - 2)->kind) && type_size(to) != 2) {
        Value *at = vsp - 2, *val = vsp - 1;
        int reg;

        if (type_size(to) == ACC_INT_SIZE) {
            /* A constant into HL, as an if and an else: as a test and then
             * force_reg either way, agondev made the R_HL for force_into
             * with sbc hl, hl -- between the test and its call z, which it
             * then always took. */
            if (val_number(val->kind)) {
                force_into(val, R_HL);
                reg = R_HL;
            } else {
                reg = force_reg(val);
            }
            if (reg == R_HL)
                out_byte(0x22);                         /* ld (nn), hl */
            else
                out_byte2(0xed, reg == R_DE ? 0x53 : 0x43); /* ld (nn), rr */
        } else {
            if (val_number(val->kind)) {
                out_byte2(0x3e, val->val);              /* ld a, n */
            } else if (val->kind == VAL_ACC) {
                ;                                       /* in A already */
            } else if (val->kind == VAL_LOCAL && !val->bits
                       && type_size(val->type) == 1) {
                ld_a_ix(val->val);                      /* no widening */
            } else {
                static const unsigned char low_of[NREGS] = { 0x7d, 0x7b, 0x79 };

                reg = force_reg(val);
                out_byte(low_of[reg]);                  /* ld a, l, e or c */
            }
            out_byte(0x32);                             /* ld (nn), a */
        }
        if (at->kind == VAL_ADDR)
            out_reloc(out_here());
        else
            gen_bss_fixup(out_here());
        out_word24(at->val);
        vstore_leave_value();

        return;
    }

    /* A constant is written through the address in HL: `*p = 0` is
     * ld (hl), 0 rather than the 0 made in HL and moved through A to (de),
     * and a three-byte one is ld de, n; ld (hl), de. The constant stays the
     * answer. */
    if (val_number((vsp - 1)->kind)) {
        int v = (vsp - 1)->val;

        force_into(vsp - 2, R_HL);
        if (type_size(to) == ACC_INT_SIZE) {
            evict_reg(R_DE);
            ld_rr_imm(R_DE, v);
            ld_ind_hl_de();
        } else {
            out_byte2(0x36, v);                 /* ld (hl), n */
            if (type_size(to) == 2) {
                inc_hl();
                out_byte2(0x36, v >> 8);
            }
        }
        vstore_leave_value();

        return;
    }

    /* Three bytes go from any register but HL through the address in HL:
     * ld (hl), de or ld (hl), bc, and the register keeps the answer. */
    if (type_size(to) == ACC_INT_SIZE) {
        Value *val = vsp - 1, *at = vsp - 2;
        int reg;

        if (val->kind == VAL_REG && val->val == R_HL
            && at->kind == VAL_REG && at->val == R_DE) {
            ex_de_hl();                 /* the two swap places */
            val->val = R_DE;
            at->val = R_HL;
        }
        force_into(at, R_HL);
        reg = force_reg(val);
        out_byte2(0xed, reg == R_DE ? 0x1f : 0x0f);    /* ld (hl), rr */
        vstore_leave_value();

        return;
    }

    /* A byte that is a byte already -- in A, a byte local, or one just read
     * and widened, the widening taken back -- written from A through the
     * pair the address is in: ld (hl), a, ld (de), a or ld (bc), a. The
     * assignment's value made after, with the mark gen_discard takes it
     * back by. `*d = *s` was the byte widened, and ld a, l before the
     * store. */
    if (type_size(to) == 1 && store_byte_through(to)) {
        conversion_from = out_here();
        force_reg(vsp - 1);
        conversion_to = out_here();
        conversion_epoch = out_rewinds;

        return;
    }

    /* The value in HL and the address in DE, which is the way round that
     * makes a narrow store the two instructions through A. */
    force_into(vsp - 1, R_HL);
    addr = force_reg(vsp - 2);
    if (addr != R_DE) {
        evict_reg(R_DE);
        mov_rr(R_DE, addr);
        (vsp - 2)->val = R_DE;
    }

    if (type_size(to) == 1) {
        ld_a_l();
        ld_de_a();
    } else {
        ld_a_l();
        ld_de_a();
        inc_de();
        ld_a_h();
        ld_de_a();
    }

    vstore_leave_value();
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

    if (top->kind == VAL_IY) {
        force_reg(top);

        return;
    }
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

        /* The step back makes the value x++ has, which `x++;` throws away:
         * marked as an assignment's conversion is, for gen_discard. */
        conversion_from = out_here();
        vstep(op == TK_PLUS ? TK_MINUS : TK_PLUS, type);
        conversion_to = out_here();
        conversion_epoch = out_rewinds;

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

    /* Stepped where it is and stepped back, as a local's is, which needs
     * no copy of the old value kept -- in the frame, which made one for
     * it: `n++;` of a global, or of a member through a pointer, took a
     * frame and a store into it. The step back is marked for gen_discard,
     * and narrowed to what was stored, which takes it back to the old
     * value whatever the store truncated. Not for a _Bool, whose old value
     * the new one does not say, nor a bit-field, which is narrower than
     * its type; nor where the step back costs more than the copy. */
    if (target != TY_BOOL && !(vsp - 1)->bits && !type_wide(target)
        && !type_float(target)
        && (!type_pointer(target) || type_size(type_deref(target)) <= STEP_MAX)) {
        vprefix_indirect(op);
        gen_effects--;                  /* counted once, above */
        conversion_from = out_here();
        vstep(op == TK_PLUS ? TK_MINUS : TK_PLUS, target);
        if (type_size(target) < ACC_INT_SIZE)
            vconvert(target);
        conversion_to = out_here();
        conversion_epoch = out_rewinds;

        return;
    }

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
