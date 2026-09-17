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
#include "rt_helpers.h"

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

static int  spill_slot(void);
static void evict_reg(int reg);
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

static int frame_size(void)
{
    return locals_size + spill_peak;
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

    force_into(vsp - 1, R_HL);
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

    /* Nothing is spilled once nothing is on the stack, so the scratch area
     * starts again. This is where the reuse happens, and it needs no help
     * from the parser: an empty value stack *is* a statement boundary. */
    if (vtop == 0)
        spill_used = 0;
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
/* Named by pointer rather than by depth from the top. Every caller says
 * `vsp - 1` or `vsp - 2`, which is a constant offset; a depth has to be
 * turned into an address, and scaling an index is a helper call here. */
static void force_into(Value *target, int want)
{
    Value *entry;

    for (entry = vstack; entry < vsp; entry++) {
        if (entry == target)
            continue;
        if (entry->kind == VAL_REG && entry->val == want) {
            int reg = reg_alloc_other(want);

            mov_rr(reg, want);
            entry->val = reg;
        }
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

void vbinop(int op)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;
    int folded, right;
    Type result, lhs_type;

    if (vtop < 2)
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
        right = force_reg_at(0);
    }

    lhs_type = type_promote(lhs->type);
    result = either_unsigned(lhs, rhs) ? TY_UINT : TY_INT;

    /* A shift's result takes its type from the left operand alone: `1u >> x`
     * is unsigned and `1 >> u` is not, which is C's rule and not the usual
     * arithmetic conversions. */
    if (op == TK_SHL || op == TK_SHR)
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

static void ld_a_imm(int value)  { out_byte(0x3e); out_byte(value & 0xff); }
static void ld_a_ix_b(int disp)  { out_byte(0xdd); out_byte(0x7e); out_byte(disp); }

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

int vnarrow_ready(int op, Type to)
{
    const Value *lhs = vsp - 2;
    const Value *rhs = vsp - 1;

    if (vtop < 2 || type_size(to) != 1)
        return 0;                       /* bytes only for now; short is wider
                                         * than A and needs a pair */
    if (op == TK_SHL || op == TK_SHR) {
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

void vbinop_narrow(int op, Type to)
{
    Value *lhs = vsp - 2;
    Value *rhs = vsp - 1;

    /* The left operand into A, unless it is already there. */
    if (lhs->kind == VAL_CONST)
        ld_a_imm(lhs->val);
    else if (lhs->kind == VAL_LOCAL)
        ld_a_ix_b(lhs->val);

    if (op == TK_SHL || op == TK_SHR) {
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
        vpush_const(folded, TY_INT);    /* a comparison is an int either way */

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

    force_into(vsp - 2, R_HL);
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

    out_byte(0xcd);                              /* call nn */
    rt_fixups[nrt_fixups].which = (unsigned char) which;
    rt_fixups[nrt_fixups].at = out_here();
    nrt_fixups++;
    out_word24(0);
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

/* A slot for a spilled register, which lasts until the end of the statement.
 * They are handed out in order and all released together, so this is a
 * high-water mark and not a free list -- there is nothing to free, since the
 * whole area goes at once. */
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

    (void) nparams;

    sym_at(fn)->val = out_here();
    vtop = 0;
    vsp = vstack;
    locals_size = 0;
    spill_used = 0;
    spill_peak = 0;

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

    out_patch24(frame_patch, -frame_size());
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

        /* A one-byte result goes in A. HL keeps the widened value as well,
         * which costs one byte and is what lets a call to a function defined
         * further down the file -- where the return type is not known yet --
         * still read its answer. */
        if (RETURNS_IN_A(return_type))
            ld_a_l();
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

    /* Read the answer from where the callee's type says it is. */
    if (RETURNS_IN_A(sym_at(fn)->type)) {
        Type returns = sym_at(fn)->type;

        if (type_unsigned(returns))
            fill_hl_with_zero();
        else
            fill_hl_with_sign_of_a();
        ld_l_a();
    }
    vpush_reg(R_HL);
    (vsp - 1)->type = type_promote(sym_at(fn)->type);
}
