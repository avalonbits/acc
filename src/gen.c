/*
 * eZ80 code generation.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 *
 * Values are not turned into instructions when they are parsed, only when
 * something needs them in a register. A constant stays a number, a local
 * stays a frame offset, and `x + 1` loads x once and adds an immediate rather
 * than loading both and adding registers.
 */

#include <stdio.h>
#include <stdlib.h>

#include "acc.h"

/* ------------------------------------------------------------------ */
/* instructions                                                        */

/* Z80 numbers a register pair in a two-bit field: BC 0, DE 1, HL 2. acc
 * numbers them HL, DE, BC so that HL -- the one everything returns in -- is
 * register zero. This maps between the two. */
static const unsigned char reg_code[NREGS] = { 2, 1, 0 };

static void ld_rr_imm(int reg, int imm)    /* ld rr, nn */
{
    out_byte(0x01 + reg_code[reg] * 0x10);
    out_word24(imm);
}

static void ld_rr_ix(int reg, int disp)    /* ld rr, (ix+d) */
{
    out_byte(0xdd);
    out_byte(0x07 + reg_code[reg] * 0x10);
    out_byte(disp);
}

static void ld_ix_rr(int disp, int reg)    /* ld (ix+d), rr */
{
    out_byte(0xdd);
    out_byte(0x0f + reg_code[reg] * 0x10);
    out_byte(disp);
}

static void push_rr(int reg) { out_byte(0xc5 + reg_code[reg] * 0x10); }
static void pop_rr(int reg)  { out_byte(0xc1 + reg_code[reg] * 0x10); }

static void add_hl_rr(int reg) { out_byte(0x09 + reg_code[reg] * 0x10); }
static void sbc_hl_rr(int reg) { out_byte(0xed); out_byte(0x42 + reg_code[reg] * 0x10); }
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
static void need_disp(int d)
{
    if (d < -128 || d > 127)
        acc_error("this function's frame is too large: a local at %d is out "
                  "of reach of (ix+d), which spans -128 to 127", d);
}

static void evict_reg(int reg);
static void force_into(int depth, int want);

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

static void ld_a_ix(int disp)   { out_byte(0xdd); out_byte(0x7e); out_byte(disp); }
static void ld_e_ix(int disp)   { out_byte(0xdd); out_byte(0x5e); out_byte(disp); }
static void ld_l_ix(int disp)   { out_byte(0xdd); out_byte(0x6e); out_byte(disp); }
static void ld_h_ix(int disp)   { out_byte(0xdd); out_byte(0x66); out_byte(disp); }
static void ld_ix_a(int disp)   { out_byte(0xdd); out_byte(0x77); out_byte(disp); }
static void ld_ix_l(int disp)   { out_byte(0xdd); out_byte(0x75); out_byte(disp); }
static void ld_ix_h(int disp)   { out_byte(0xdd); out_byte(0x74); out_byte(disp); }
static void ld_l_a(void)        { out_byte(0x6f); }
static void ld_h_a(void)        { out_byte(0x67); }
static void ld_a_l(void)        { out_byte(0x7d); }
static void ld_a_h(void)        { out_byte(0x7c); }
static void ld_e_l(void)        { out_byte(0x5d); }
static void ld_l_e(void)        { out_byte(0x6b); }
static void rlc_l(void)         { out_byte(0xcb); out_byte(0x05); }
static void sbc_hl_hl(void)     { out_byte(0xed); out_byte(0x62); }

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

    ld_e_ix(disp);              /* low byte kept aside */
    ld_a_ix(disp + 1);          /* high byte decides the sign */
    fill_hl_with_sign_of_a();
    ld_h_a();
    ld_l_e();
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

    ld_a_h();
    ld_e_l();
    if (type_unsigned(to))
        fill_hl_with_zero();
    else
        fill_hl_with_sign_of_a();
    ld_h_a();
    ld_l_e();
}

/* The type this function was declared to return, so that `char f()` giving
 * back 300 gives back 44 as C says it must. */
static Type return_type = TY_INT;

/* ------------------------------------------------------------------ */
/* the value stack                                                     */

#define VSTACK_MAX 64

static Value vstack[VSTACK_MAX];
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
static int frame_size;
static int frame_patch;          /* where the prologue's frame size is written */

static void vcheck(void)
{
    /* The two have to agree, and nothing but a bug can make them disagree.
     * Checked in the sanitized build the tests run, not in the compiler:
     * this is on the path of every value the parser produces. */
#ifdef ACC_CHECK_VSTACK
    if (vsp != vstack + vtop)
        acc_error("internal: the value stack pointer and its count disagree");
#endif
    if (vtop >= VSTACK_MAX)
        acc_error("expression is nested too deeply");
}

/* The one place that writes an entry and moves the top, so that the pointer
 * and the count cannot get out of step anywhere else. */
static void vpush(int kind, Type type, int val)
{
    vcheck();
    vsp->kind = (unsigned char) kind;
    vsp->type = type;
    vsp->val = val;
    vsp++;
    vtop++;
}

void vpush_const(int val)
{
    vpush(VAL_CONST, TY_INT, val);
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

    if (vtop <= 0)
        acc_error("internal: nothing to convert");
    if (type_size(to) >= ACC_INT_SIZE) {
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

    force_into(0, R_HL);
    convert_in_hl(to);
    top->type = type_promote(to);
}

Type vtype(void)
{
    if (vtop <= 0)
        acc_error("internal: asked the type of nothing");

    return (vsp - 1)->type;
}

void vdrop(void)
{
    if (vtop <= 0)
        acc_error("internal: value stack underflow");
    vtop--;
    vsp--;
}

static int reg_busy(int reg)
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
 * expression being compiled is working at the top of the stack. */
static void spill_one(void)
{
    int i;

    for (i = 0; i < vtop; i++) {
        if (vstack[i].kind == VAL_REG) {
            int off = gen_local(ACC_INT_SIZE);

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
            int off = gen_local(ACC_INT_SIZE);

            need_disp(off);
            ld_ix_rr(off, vstack[i].val);
            vstack[i].kind = VAL_LOCAL;
            vstack[i].val = off;
        }
    }
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
    spill_one();
    for (reg = 0; reg < NREGS; reg++)
        if (reg != avoid && !reg_busy(reg))
            return reg;
    acc_error("internal: no register after spilling");

    return 0;
}

/* Materialises the entry at `depth` below the top into a register and returns
 * it. Anything already in a register stays where it is. */
static int force_reg_at(int depth)
{
    Value *val = vsp - 1 - depth;
    int reg;

    if (val->kind == VAL_REG)
        return val->val;

    reg = reg_alloc();
    if (val->kind == VAL_CONST) {
        ld_rr_imm(reg, val->val);
    } else {
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

/* Moves every value out of `reg`, so that it can be written without losing
 * anything the expression still needs. */
static void evict_reg(int reg)
{
    int i;

    for (i = 0; i < vtop; i++) {
        if (vstack[i].kind == VAL_REG && vstack[i].val == reg) {
            int to = reg_alloc_other(reg);

            mov_rr(to, reg);
            vstack[i].val = to;
        }
    }
}

/* Materialises the entry at `depth` into one particular register, moving
 * whatever else is living there out of the way first. */
static void force_into(int depth, int want)
{
    int idx = vtop - 1 - depth;
    int i;

    for (i = 0; i < vtop; i++) {
        if (i == idx)
            continue;
        if (vstack[i].kind == VAL_REG && vstack[i].val == want) {
            int reg = reg_alloc_other(want);

            mov_rr(reg, want);
            vstack[i].val = reg;
        }
    }
    (void) i;

    if (vstack[idx].kind == VAL_REG) {
        if (vstack[idx].val != want)
            mov_rr(want, vstack[idx].val);
    } else if (vstack[idx].kind == VAL_CONST) {
        ld_rr_imm(want, vstack[idx].val);
    } else {
        need_disp(vstack[idx].val);
        if (type_size(vstack[idx].type) < ACC_INT_SIZE)
            load_narrow_into(want, vstack[idx].val, vstack[idx].type);
        else
            ld_rr_ix(want, vstack[idx].val);
    }
    vstack[idx].kind = VAL_REG;
    vstack[idx].type = type_promote(vstack[idx].type);
    vstack[idx].val = want;
}

int vpop_reg(void)
{
    int reg = force_reg_at(0);

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

void vbinop(int op)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;
    int folded, right;
    Type result;

    if (vtop < 2)
        acc_error("internal: binary operator with nothing to work on");

    /* Both sides known: the answer is known, and nothing is emitted. */
    if (lhs->kind == VAL_CONST && rhs->kind == VAL_CONST
        && const_fold(op, lhs->val, rhs->val, &folded)) {
        vdrop();
        vdrop();
        vpush_const(folded);

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
    force_into(1, R_HL);
    right = force_reg_at(0);

    result = either_unsigned(lhs, rhs) ? TY_UINT : TY_INT;

    switch (op) {
    case TK_PLUS:
        add_hl_rr(right);
        break;

    case TK_MINUS:
        or_a_a();               /* sbc reads the carry, so clear it */
        sbc_hl_rr(right);
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

    if (type_size(type) < ACC_INT_SIZE) {
        /* The narrow stores write out of HL, so the value goes there. */
        force_into(0, R_HL);
        vconvert(type);
        need_disp(offset);
        store_narrow(offset, type);

        return;
    }

    reg = force_reg_at(0);
    need_disp(offset);
    ld_ix_rr(offset, reg);
}

void vneg(void)
{
    Value *top = vsp - 1;
    int right;

    if (top->kind == VAL_CONST) {
        top->val = trunc_int(-top->val);

        return;
    }

    /* 0 - x, so the operand goes anywhere but HL and HL is then cleared of
     * whatever else was in it -- the zero is about to overwrite it. */
    if (!(top->kind == VAL_REG && top->val != R_HL))
        force_into(0, reg_alloc_other(R_HL));
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
    /* ~x is -x - 1, which needs no instruction this chip does not have. */
    vneg();
    vpush_const(1);
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
    int  at;
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
    nfixups++;
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

void vcmp(int op)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;
    int folded, right, is_unsigned;

    if (vtop < 2)
        acc_error("internal: a comparison with nothing to compare");

    is_unsigned = either_unsigned(lhs, rhs);

    if (lhs->kind == VAL_CONST && rhs->kind == VAL_CONST
        && !is_unsigned
        && const_fold(op, lhs->val, rhs->val, &folded)) {
        vdrop();
        vdrop();
        vpush_const(folded);

        return;
    }

    /* `a > b` is `b < a`, and `a <= b` is `b >= a`. Swapping costs nothing
     * here: both sides are still descriptions on a stack, not registers. */
    if (op == TK_GT || op == TK_LE) {
        Value swapped = *lhs;

        *lhs = *rhs;
        *rhs = swapped;
        op = (op == TK_GT) ? TK_LT : TK_GE;
    }

    force_into(1, R_HL);
    right = force_reg_at(0);

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

    out_byte(op);
    hole = out_here();
    out_word24(0);

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
    out_byte(JP_ANY);
    out_word24(target);
}

int gen_jump_if_false(void)
{
    int reg = vpop_reg();

    if (reg != R_HL)
        mov_rr(R_HL, reg);

    /* There is no "is this register zero" instruction for a 24-bit value.
     * The upper byte of HL is not addressable, so the 16-bit idiom -- `ld a,l`
     * then `or a,h` -- would test two thirds of the value and call 0x010000
     * false. Subtracting zero tests all of it.
     *
     * This clobbers BC, which is free because the stack is empty here: the
     * condition was the only thing on it and it has just been popped. */
    if (vtop != 0)
        acc_error("internal: %d values still live at a branch", vtop);
    ld_rr_imm(R_BC, 0);
    or_a_a();
    sbc_hl_rr(R_BC);

    return jump_op(JP_Z);
}

void gen_label(int hole)
{
    patch_to_here(hole);
}

void gen_finish(void)
{
    int i;

    for (i = 0; i < nfixups; i++) {
        Sym *fn = sym_at(fixups[i].fn);

        if (!fn->val)
            acc_error("'%s' is called but never defined", name_text(fn->name));
        out_patch24(fixups[i].at, fn->val);
    }
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
    frame_size += size;

    return -frame_size;
}

void gen_func_begin(int fn, int nparams, Type returns)
{
    return_type = returns;

    (void) nparams;

    sym_at(fn)->val = out_here();
    vtop = 0;
    vsp = vstack;
    frame_size = 0;

    /* push ix / ld ix, 0 / add ix, sp -- the frame agondev's __frameset
     * builds, written out rather than called, because there is nothing to
     * link against yet. ix then points at the saved ix, so the first argument
     * is at ix+6: three bytes of saved ix and three of return address. */
    out_byte(0xdd); out_byte(0xe5);              /* push ix */
    out_byte(0xdd); out_byte(0x21);              /* ld ix, 0 */
    out_word24(0);
    out_byte(0xdd); out_byte(0x39);              /* add ix, sp */

    /* ld hl, -frame / add hl, sp / ld sp, hl. The size is not known until the
     * body has been read, so the space is reserved and filled in at the end. */
    out_byte(0x21);                              /* ld hl, nn */
    frame_patch = out_here();
    out_word24(0);
    out_byte(0x39);                              /* add hl, sp */
    out_byte(0xf9);                              /* ld sp, hl */
}

void gen_func_end(void)
{
    /* Restoring sp from ix unconditionally costs two bytes in a function with
     * no locals and saves the epilogue having to know the frame size. */
    out_byte(0xdd); out_byte(0xf9);              /* ld sp, ix */
    out_byte(0xdd); out_byte(0xe1);              /* pop ix */
    out_byte(0xc9);                              /* ret */

    out_patch24(frame_patch, -frame_size);
}

void gen_return(void)
{
    /* The result goes in HL, which is where agondev returns an int as well --
     * worth matching even with nothing to link against, because it is what
     * lets the two be mixed later. */
    if (vtop > 0) {
        int reg;

        vconvert(return_type);
        reg = vpop_reg();
        if (reg != R_HL)
            mov_rr(R_HL, reg);
    }
    out_byte(0xdd); out_byte(0xf9);              /* ld sp, ix */
    out_byte(0xdd); out_byte(0xe1);              /* pop ix */
    out_byte(0xc9);                              /* ret */
}

/* Arguments are pushed right to left, each in a whole three-byte slot, and
 * the caller takes them off again -- which is agondev's convention. */
void gen_call(int fn, int nargs)
{
    int i;

    /* Anything still live in a register has to come out before the call.
     * The result comes back in HL and the callee is free with the rest, so a
     * value left in one does not survive -- which is how `f(..) - g(..)` lost
     * f's answer the moment g was called. The arguments are exempt: they are
     * about to be pushed and consumed. */
    save_regs_below(nargs);

    for (i = 0; i < nargs; i++) {
        int reg = vpop_reg();

        push_rr(reg);
    }

    out_byte(0xcd);                              /* call nn */
    if (sym_at(fn)->val) {
        out_word24(sym_at(fn)->val);
    } else {
        /* Defined further down the file, or not at all. The site is recorded
         * and filled in once the whole file has been read; gen_finish says so
         * if it never was. */
        fixup_add(fn, out_here());
        out_word24(0);
    }

    for (i = 0; i < nargs; i++)
        pop_rr(R_DE);                            /* discard, cheapest form */

    vpush_reg(R_HL);
}
