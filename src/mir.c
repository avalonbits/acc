/*
 * opt-acc's machine-level backend: docs/machine-ir-backend.md.
 *
 * A function's SSA form (ssa.c) is selected into instructions of the eZ80
 * whose operands are virtual registers, the registers are allocated over
 * them, and the bytes are made after. Milestones 1 and 2: functions of
 * ints, pointers and chars, and their calls of functions named directly.
 * Anything else is declined (mir_ok), and the function goes to the other
 * ways the pick weighs.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifdef OPT_ACC

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "gen_int.h"
#include "out_int.h"
#include "runtime.h"
#include "genlog.h"

#define GENLOG_OPS
#include "genlog_calls.h"
#undef GENLOG_OPS

#include "ssa_int.h"

/* ------------------------------------------------------------------ */
/* registers                                                           */

/* The physical registers a value may be in: the bytes, and the pairs.
 * IX is the frame's. */
enum { P_A, P_B, P_C, P_D, P_E, P_H, P_L, P_BC, P_DE, P_HL, P_IY, NPREGS };

/* Their units -- the bytes they are made of, U the top byte of a pair that
 * only an ADL instruction reaches -- which is what two values in registers
 * may not share. F is a unit too: a comparison's flags, live until the
 * branch on them. */
enum { U_A, U_B, U_C, U_D, U_E, U_H, U_L, U_BU, U_DU, U_HU, U_IYL, U_IYH,
       U_IYU, U_F };

#define UB(u) (1u << (u))

static const unsigned preg_units[NPREGS] = {
    UB(U_A), UB(U_B), UB(U_C), UB(U_D), UB(U_E), UB(U_H), UB(U_L),
    UB(U_B) | UB(U_C) | UB(U_BU), UB(U_D) | UB(U_E) | UB(U_DU),
    UB(U_H) | UB(U_L) | UB(U_HU), UB(U_IYL) | UB(U_IYH) | UB(U_IYU)
};

/* The 8-bit register codes the instructions are made with. */
static const int r8_code[] = { 7, 0, 1, 2, 3, 4, 5 };

#define PB(p) (1u << (p))
#define C_A    PB(P_A)
#define C_R8   (PB(P_A) | PB(P_B) | PB(P_C) | PB(P_D) | PB(P_E) | PB(P_H) | PB(P_L))
#define C_HL   PB(P_HL)
#define C_DE   PB(P_DE)
#define C_BC   PB(P_BC)
#define C_IY   PB(P_IY)
#define C_O24  (PB(P_DE) | PB(P_BC))
#define C_P24  (PB(P_HL) | PB(P_DE) | PB(P_BC))
#define C_R24  (PB(P_HL) | PB(P_DE) | PB(P_BC) | PB(P_IY))

/* The low byte of a pair, as an 8-bit register: a byte read of it. */
static int low_of(int preg)
{
    switch (preg) {
    case P_BC: return P_C;
    case P_DE: return P_E;
    case P_HL: return P_L;
    }

    return -1;
}

/* ------------------------------------------------------------------ */
/* the machine IR                                                      */

/* What an instruction is. `d` is the register it writes, `a` and `b` those
 * it reads -- virtual registers until allocation -- `imm` a constant or a
 * displacement, `sym` a symbol the link fills in. */
enum {
    M_COPY,         /* d = a, the same width */
    M_LDI,          /* d = imm */
    M_LDSYM,        /* d = &sym + imm (24) */
    M_LDA,          /* d = an address in the image or the bss, imm, as the
                     * constant's kind imm2 (VAL_ADDR, VAL_BSS) has it */
    M_DATA,         /* a string's bytes, jumped over: the SSA instruction ssa */
    M_LDF,          /* d = (ix+imm) */
    M_STF,          /* (ix+imm) = a */
    M_STFI,         /* (ix+imm) = imm2, a byte */
    M_LEAF,         /* d = ix+imm (24) */
    M_LDP,          /* d = (a+imm): a in HL, or IY; a byte at imm 0 into A
                     * through BC or DE too */
    M_STP,          /* (a+imm) = b: likewise */
    M_STPI,         /* (a+imm) = imm2, a byte */
    M_STEPP,        /* (a+imm) stepped by imm2, +1 or -1, a byte: inc or dec
                     * (hl) or (iy+d) */
    M_LDG,          /* d = (sym+imm) */
    M_STG,          /* (sym+imm) = a */
    M_ADD24,        /* d = a + b: d and a HL, b HL, DE or BC */
    M_SUB24,        /* d = a - b: or a / sbc hl, rr */
    M_STEP24,       /* d = a + imm, -4..4, inc or dec, d and a the same */
    M_BYTES24,      /* d = a op imm2 (&, | or ^, imm), its low two bytes
                     * worked on where in place: d and a BC, DE or HL, the
                     * same; A, `t`, clobbered where a byte goes through it */
    M_NEG24,        /* d = -a, d and a HL, DE clobbered */
    M_NOT24,        /* d = ~a, likewise */
    M_ALU8,         /* d = a op b: d and a A; imm the operator */
    M_ALU8I,        /* d = a op imm2 */
    M_CMP24,        /* flags of a - b, unsigned: a HL, clobbered */
    M_CMP24S,       /* carry when a < b as signed: a HL, clobbered, b DE */
    M_CMP24SI,      /* flags of a - imm2 as signed: carry less; a HL, clobbered,
                     * and DE, `t`, but against 0, the sign by add hl, hl */
    M_TST24,        /* Z when a is 0: a HL, kept */
    M_CMP8,         /* flags of a - b: a A */
    M_CMP8I,        /* flags of a - imm2 */
    M_BOOL,         /* d = 1 when the flags say imm (a condition), else 0 */
    M_ZEXT,         /* d (24) = a (8), zero-extended */
    M_SEXT,         /* d (24) = a (8), sign-extended: d HL, a A */
    M_TRUNC,        /* d (8) = a's low byte */
    M_HELPER,       /* d = routine imm (a, b): HL, BC -> HL; A, F clobbered */
    M_BR,           /* to block imm2 when the flags say imm, else to the next */
    M_JMP,          /* to block imm2 */
    M_RET,          /* return a, in HL, as `type`, imm set where it is the
                     * function's answer already; or nothing; or with imm2,
                     * the SSA return's constant */
    M_PCOPY,        /* the parallel copies an edge makes: see pcopy */
    M_SAVE,         /* before a call's arguments: the pairs imm2 says pushed */
    M_PUSH,         /* an argument: a pushed, a pair */
    M_CALL,         /* d = sym (imm slots pushed), in HL or A; then the slots
                     * popped, and the pairs imm2 says -- BC 1, DE 2, IY 4,
                     * those live across it -- popped back. A, F and HL
                     * clobbered: the callee may change BC, DE and IY too,
                     * which is what the pushing and popping is for */
    NMOPS
};

typedef struct {
    int op;
    int d, a, b;                /* virtual registers, or -1 */
    int t;                      /* a register it clobbers, as a virtual one:
                                 * the bias's BC, NEG's DE -- or -1 */
    int kills;                  /* operands it changes, a bit each: a, b */
    int ssa;                    /* M_RET: the SSA return it makes */
    int imm, imm2;
    int sym;                    /* fixup symbol, or -1 */
    int width;                  /* 1 or 3: of a load's or a store's value */
    Type type;                  /* M_RET: what the function answers */
} MIns;

/* A virtual register: its width, the registers it may be in, and what it
 * is when made again rather than kept -- a constant, an address, its
 * parameter's slot. */
typedef struct {
    int width;
    unsigned cls;
    int preg;                   /* after allocation, or -1 */
    int spill;                  /* its frame slot when spilled, or 0 */
    int remat;                  /* 0, or M_LDI / M_LDSYM / M_LEAF / M_LDF */
    int remat_imm, remat_sym;
    int param;                  /* a parameter's value: its slot, never stored */
    int hint;                   /* a register a copy would rather it were */
    long weight;                /* uses and defs, weighted by loop depth */
    int short_lived;            /* made for one instruction: no spill helps */
    int ext;                    /* the byte it is that byte widened from, or -1 */
    int ext_signed;             /* and whether by its sign */
    unsigned forbid;            /* units it may not be in: clobbered while live */
} VReg;

typedef struct {
    MIns *ins;
    int   n, cap;
    int   ssa_block;            /* the SSA block, or -1 for an edge's */
    int   succ[2], nsucc;
    int   addr;                 /* where it was made */
} MBlock;

/* Parallel copies: each edge's, kept apart from the instructions so that
 * they are made after allocation, all read before any is written. */
typedef struct {
    int *dst, *src, n;
} PCopy;

#define MAX_PCOPY 64            /* a join with more phis is declined */

static MBlock *mb;
static int     nmb, mb_cap;
static VReg   *vr;
static int     nvr, vr_cap;
static PCopy  *pc;
static int     npc, pc_cap;
static int    *val_vr;          /* an SSA value's virtual register, or -1 */
static int    *ssa_mb;          /* an SSA block's machine block */
static unsigned char *multi_def; /* by value: made in more than one place */
static int     cur;             /* the block being selected into */
static const char *mir_why;     /* why the function is declined */
static const char *sel_fail;    /* why selection gave up */

/* By value: how many operands and phis read it, and the instruction
 * that does where only one does -- counted once, in setup. */
static int *use_n, *user_of;

/* By machine block: the edge blocks its branch jumps to, made after it. */
static int *taken_head, *taken_next;

static int new_vr(int width, unsigned cls)
{
    GROW(vr, nvr, vr_cap);
    memset(&vr[nvr], 0, sizeof vr[nvr]);
    vr[nvr].width = width;
    vr[nvr].cls = cls;
    vr[nvr].preg = -1;
    vr[nvr].hint = -1;
    vr[nvr].ext = -1;

    return nvr++;
}

static int new_mb(int ssa_block)
{
    GROW(mb, nmb, mb_cap);
    memset(&mb[nmb], 0, sizeof mb[nmb]);
    mb[nmb].ssa_block = ssa_block;

    return nmb++;
}

static MIns *emit_mi(int op)
{
    MBlock *blk = &mb[cur];
    MIns *mi;

    GROW(blk->ins, blk->n, blk->cap);
    mi = &blk->ins[blk->n++];
    memset(mi, 0, sizeof *mi);
    mi->op = op;
    mi->d = mi->a = mi->b = mi->t = mi->sym = -1;

    return mi;
}

static MIns *mi3(int op, int d, int a, int b)
{
    MIns *mi = emit_mi(op);

    mi->d = d;
    mi->a = a;
    mi->b = b;

    return mi;
}

/* ------------------------------------------------------------------ */
/* what is made here                                                   */

/* The types the code here holds: an int or a pointer in a pair, a char or
 * a _Bool in a byte. Not a short, a long, a float or a struct. */
static int mir_type(Type type)
{
    if (type == TY_BOOL)
        return 1;
    if (type == TY_VOID || type_is_struct(type) || type_wide(type)
        || type_float(type))
        return 0;

    return type_size(type) == 1 || type_size(type) == ACC_INT_SIZE;
}

static int width_of(Type type)
{
    return type_size(type) == 1 || type == TY_BOOL ? 1 : 3;
}

/* A constant that is a number, which the code here reasons about; and one
 * that is an address -- a string's in the image, a static's in the bss --
 * which it only loads, where the link moves it. */
static int is_num(const Ent *ent)
{
    return ent->val == S_CONST && ent->attr.kind == VAL_CONST;
}

static int is_addr(const Ent *ent)
{
    return ent->val == S_CONST
           && (ent->attr.kind == VAL_ADDR || ent->attr.kind == VAL_BSS);
}

static int mir_operand_ok(const Ent *ent)
{
    /* An address held as a value: made by a register the code here would
     * load with the number, unmoved -- not taken. */
    if (ent->val >= 0 && (ent->attr.kind == VAL_ADDR || ent->attr.kind == VAL_BSS))
        return 0;
    if (is_addr(ent))
        return mir_type(ent->attr.type) && width_of(ent->attr.type) == 3
               && !ent->attr.bits;      /* a bit-field's: read as one */
    if (ent->val == S_CONST)
        return ent->attr.kind == VAL_CONST && mir_type(ent->attr.type);
    if (ent->val < 0)
        return 0;

    return mir_type(vals[ent->val].type) && mir_type(ent->attr.type)
           && !ent->attr.bits;
}

/* Whether a call is one made here: of a function named directly, not
 * setjmp -- whose second return finds the registers pushed around it long
 * gone -- nor longjmp, nor one gen_call makes in place of a call, memcpy
 * and the rest with ldir and exit, which would be the library's slower
 * C here; answering void or a type held here, its arguments each one held
 * here and, where it has a parameter, of a type held here too; a _Bool's
 * only from a _Bool, since anything else would be tested, not narrowed. */
static int call_ok(const Ins *insn)
{
    static NameRef setjmp_name;
    int fn = (int) insn->rec->arg[0], first = (int) insn->rec->arg[2];
    int nparams = (int) insn->rec->arg[3], arg;
    const Sym *callee = sym_at(fn);

    static const char *const in_place[] = {
        "memcpy", "memmove", "memset", "memchr", "exit", "longjmp", NULL
    };

    if (!setjmp_name)
        setjmp_name = name_intern("setjmp", 6);
    if (callee->name == setjmp_name)
        return mir_why = "a call of setjmp", 0;
    for (arg = 0; in_place[arg]; arg++)
        if (!strcmp(name_text(callee->name), in_place[arg]))
            return mir_why = "a call gen_call makes in place", 0;
    if (callee->type != TY_VOID && !mir_type(callee->type))
        return mir_why = "a call answering a type not held here", 0;
    for (arg = 0; arg != insn->nin; arg++) {
        Type param = arg < nparams ? sym_param_type(first, arg) : TY_INT;

        if (!mir_type(param))
            return mir_why = "an argument of a type not held here", 0;
        if (param == TY_BOOL && insn->in[arg].attr.type != TY_BOOL)
            return mir_why = "an argument made a _Bool", 0;
    }

    return 1;
}

/* Whether the function is one milestones 1 and 2 make. */
static int mir_ok(void)
{
    int at, operand, phi;

    mir_why = NULL;
    if (cached_any)
        return mir_why = "a cached local", 0;
    if (nraws || nmerged)
        return mir_why = "a block's statics, or an inlined body's room", 0;
    for (phi = 0; phi != nphis; phi++)
        if (phis[phi].live && !mir_type(vals[phis[phi].val].type))
            return mir_why = "a phi not of an int, a pointer or a char", 0;
    for (at = 0; at != nlocals; at++)
        if (locals[at].ok && locals[at].is_param
            && !disp_fits(inline_moved(locals[at].offset) + ACC_INT_SIZE - 1))
            return mir_why = "a parameter past (ix+d)'s reach", 0;
    for (at = 1; at != ninsns; at++) {
        const Ins *insn = &insns[at];

        if (insn->rec && insn->rec->top.bits)
            return mir_why = "a bit-field", 0;
        for (operand = 0; operand != insn->nin; operand++)
            if (!mir_operand_ok(&insn->in[operand]))
                return mir_why = "an operand not of an int, a pointer or a char", 0;
        if (insn->res >= 0 && vals[insn->res].used && !mir_type(vals[insn->res].type))
            return mir_why = "a value not of an int, a pointer or a char", 0;
        /* What is converted to, stepped or narrowed to: a type held here. */
        if (((insn->op == GL_vconvert || insn->op == GL_vcast)
             && !mir_type((Type) insn->rec->arg[0]))
            || ((insn->op == I_CONV || insn->op == I_STEP) && !mir_type(insn->local_type))
            || (insn->op == I_SET && insn->target >= 0 && !mir_type(vals[insn->target].type))
            || (insn->op == GL_vapply && insn->rec->arg[1]
                && type_size((Type) insn->rec->arg[1]) != 1))
            return mir_why = "a conversion to a type not held here", 0;
        switch (insn->op) {
        case I_FRAME:
            /* A declaration of a local that is values now lays down
             * nothing; a claim of IY only for a `register` one. */
            switch (insn->rec->op) {
            case GL_gen_local: case GL_gen_local_array:
            case GL_gen_local_array_size: case GL_gen_local_far:
            case GL_gen_local_scope:
                continue;
            case GL_gen_iy_claim:
                if (!((int) insn->rec->arg[2] & SQ_REGISTER))
                    continue;
                break;
            }
            if (!frame_of_value(insn))
                return mir_why = "a local in memory", 0;
            continue;
        case GL_gen_data:
            continue;
        case GL_vdrop: case GL_gen_stmt_end: case GL_gen_value_end:
        case GL_vpush_const: case I_BR: case I_JMP: case I_SET: case I_CONV:
        case GL_gen_return: case GL_vpush_global_addr: case GL_vneg:
        case GL_vnot: case GL_vtruth: case GL_vconvert: case GL_vcast:
        case GL_vmember:
            continue;
        case GL_vprefix_indirect: case GL_vpostfix_indirect: {
            /* ++ and -- through a pointer: of an int, a char or a pointer
             * whose step is a size known here, not a VLA's -- not a _Bool,
             * which is set, not stepped. */
            Type to = type_pointer(insn->in[0].attr.type)
                      ? type_deref(insn->in[0].attr.type) : TY_VOID;
            int ext = insn->in[0].attr.ext;

            if (!mir_type(to) || to == TY_BOOL
                || (type_pointer(to)
                    && ((ext && ext_variably_modified(ext))
                        || type_step(to, ext) <= 0)))
                return mir_why = "a step through a pointer not held here", 0;
            continue;
        }
        case GL_vderef: case GL_vstore_indirect: {
            /* What is read or written: a type held here -- or, read, a
             * struct or an array, which is its address. */
            Type to = type_pointer(insn->in[0].attr.type)
                      ? type_deref(insn->in[0].attr.type) : TY_VOID;

            if (mir_type(to) || (insn->op == GL_vderef
                                 && (type_is_struct(to) || type_is_array(to))))
                continue;
            return mir_why = "a read or write not of an int, a pointer or a char", 0;
        }
        case I_STEP:
            if (insn->local_type == TY_BOOL)
                return mir_why = "a _Bool stepped", 0;
            continue;
        case GL_gen_call:
            if (!call_ok(insn))
                return 0;
            continue;
        case GL_vapply:
            switch ((int) insn->rec->arg[0]) {
            case TK_PLUS: case TK_MINUS: case TK_STAR: case TK_SLASH:
            case TK_PERCENT: case TK_AMP: case TK_PIPE: case TK_CARET:
            case TK_SHL: case TK_SHR: case TK_LT: case TK_GT: case TK_LE:
            case TK_GE: case TK_EQ: case TK_NE:
                continue;
            }
            return mir_why = "an operator", 0;
        }
        return mir_why = "an instruction milestone 1 does not make", 0;
    }

    return 1;
}

/* ------------------------------------------------------------------ */
/* selection                                                           */

static int sel_at;              /* the SSA instruction being selected */
static void select_all(void);
static int in_class(int v, unsigned cls);
static int addr_vr(const Ent *ent);
static int skip_to;             /* a branch fused into the comparison before */

/* A constant's virtual register: made again where it is read. */
static int const_vr(int value, int width)
{
    int v = new_vr(width, width == 1 ? C_R8 : C_R24);
    MIns *mi = mi3(M_LDI, v, -1, -1);

    mi->imm = width == 1 ? value & 0xff : value & 0xffffff;
    vr[v].remat = M_LDI;
    vr[v].remat_imm = mi->imm;

    return v;
}

/* An address constant's register: made again where it is read. */
static int addrc_vr(int value, int kind)
{
    int v = new_vr(3, C_R24);
    MIns *mi = mi3(M_LDA, v, -1, -1);

    mi->imm = value;
    mi->imm2 = kind;
    vr[v].remat = M_LDA;
    vr[v].remat_imm = value;
    vr[v].remat_sym = kind;

    return v;
}

/* A byte widened to an int, by its sign or by zeros -- and the int knowing
 * the byte it is, for what wants only that. */
static int widen(int v, int by_sign)
{
    int d;

    if (by_sign) {
        d = new_vr(3, C_HL);
        mi3(M_SEXT, d, in_class(v, C_A), -1);
    } else {
        d = new_vr(3, C_P24);
        mi3(M_ZEXT, d, in_class(v, C_A), -1);
    }
    vr[d].ext = v;
    vr[d].ext_signed = by_sign;

    return d;
}

/* An operand at a width: its value's register, a constant made, or a byte
 * widened -- by the type it is read as, its own when that is a byte. */
static int operand_vr(const Ent *ent, int width)
{
    int v, w;
    Type type;

    if (is_addr(ent))
        return addrc_vr(ent->attr.val, ent->attr.kind);
    if (ent->val == S_CONST)
        return const_vr(ent->attr.val, width);
    v = val_vr[ent->val];
    if (v < 0)
        v = addr_vr(ent);               /* a folded address, wanted whole */
    w = vr[v].width;
    type = vals[ent->val].type;
    if (w == width)
        return v;
    if (w == 3 && vr[v].ext >= 0)       /* a byte widened: the byte */
        return vr[v].ext;
    if (w == 3) {                       /* a byte of a pair */
        int d = new_vr(1, C_R8);

        mi3(M_TRUNC, d, in_class(v, C_P24), -1);
        return d;
    }
    return widen(v, !(type_unsigned(type) || type == TY_BOOL));
}

/* A value's register, made for it where it is defined. */
static int def_vr(int val)
{
    return val_vr[val];
}

/* A copy into a register of a class: where an instruction wants its
 * operand, the allocator then putting both in the same register where it
 * can, and the copy coming to nothing. */
static int in_class(int v, unsigned cls)
{
    int t = new_vr(vr[v].width, cls);
    MIns *mi;

    vr[t].short_lived = 1;

    /* A constant or an address: made again, in the class wanted. */
    if (vr[v].remat == M_LDI || vr[v].remat == M_LDSYM || vr[v].remat == M_LDA) {
        mi = mi3(vr[v].remat, t, -1, -1);
        mi->imm = vr[v].remat_imm;
        if (vr[v].remat == M_LDSYM)
            mi->sym = vr[v].remat_sym;
        if (vr[v].remat == M_LDA)
            mi->imm2 = vr[v].remat_sym;
        vr[t].remat = vr[v].remat;
        vr[t].remat_imm = vr[v].remat_imm;
        vr[t].remat_sym = vr[v].remat_sym;
        return t;
    }
    mi3(M_COPY, t, v, -1);

    return t;
}

/* The result `t`, made in a register of a class, copied to the value's --
 * widened by `type`, what it was made as, or narrowed to its low byte,
 * where the value is held at another width. */
static void to_val_as(int val, int t, Type type)
{
    int v;

    if (val < 0 || val_vr[val] < 0)
        return;
    v = def_vr(val);
    if (vr[v].width == 3 && vr[t].width == 1) {
        t = widen(t, !(type_unsigned(type) || type == TY_BOOL));
    } else if (vr[v].width == 1 && vr[t].width == 3) {
        int d = new_vr(1, C_R8);

        mi3(M_TRUNC, d, in_class(t, C_P24), -1);
        t = d;
    }
    mi3(M_COPY, v, t, -1);
    if (!multi_def[val]) {              /* one definition: what it is */
        vr[v].ext = vr[t].ext;
        vr[v].ext_signed = vr[t].ext_signed;
    }
}

static void to_val(int val, int t)
{
    to_val_as(val, t, TY_INT);
}

/* An SSA value's width as the code here holds it. */
static int val_width(int val)
{
    return width_of(vals[val].type);
}

/* Whether an operand, as an int, is a byte widened -- K_ZEXT by zeros,
 * K_SEXT by its sign, a constant either or the one it fits -- or 0. */
enum { K_ZEXT = 1, K_SEXT = 2 };

/* ------------------------------------------------------------------ */
/* known bits                                                          */

/* By value: the bits of it, as an int, known to be zero -- a byte's above
 * its own where it is widened by zeros, a mask's outside it, a truth's but
 * the lowest. Worked out once, in the order the instructions are, a phi
 * knowing what all of what comes into it knows -- nothing of what comes
 * round a loop, not worked out yet -- so that it takes one pass. */
static unsigned *known_zero;
static unsigned char *known_done;

#define ALL24 0xffffffu

static unsigned type_zero(Type type)
{
    if (type == TY_BOOL)
        return ALL24 & ~1u;
    if (type_size(type) == 1 && type_unsigned(type))
        return 0xffff00u;

    return 0;
}

/* What is known of a value as its type widens it: a signed byte is 0
 * above itself only where its sign is known to be. */
static unsigned as_type(unsigned kz, Type type)
{
    kz |= type_zero(type);
    if (type_size(type) == 1 && !type_unsigned(type) && type != TY_BOOL)
        kz = kz & 0x80 ? kz | 0xffff00u : kz & 0xffu;

    return kz;
}

static unsigned ent_zero(const Ent *ent)
{
    if (is_num(ent))
        return ~(unsigned) ent->attr.val & ALL24;
    if (ent->val < 0 || !known_done[ent->val])
        return as_type(0, ent->attr.type);

    return as_type(known_zero[ent->val], ent->attr.type);
}

/* The bits above the highest that may be set: all those a value below
 * 2^24 with `kz` known zero leaves clear from the top. */
static unsigned top_zero(unsigned kz)
{
    unsigned bit, run = 0;

    for (bit = 0x800000; bit && (kz & bit); bit >>= 1)
        run |= bit;

    return run;
}

static unsigned insn_zero(const Ins *insn)
{
    unsigned l = insn->nin > 0 ? ent_zero(&insn->in[0]) : 0;
    unsigned r = insn->nin > 1 ? ent_zero(&insn->in[1]) : 0;
    int k;

    switch (insn->op) {
    case GL_vpush_const:
        return ~(unsigned) insn->rec->arg[0] & ALL24;
    case GL_vconvert: case GL_vcast:
        return type_zero((Type) insn->rec->arg[0])
               | (type_size((Type) insn->rec->arg[0]) == ACC_INT_SIZE ? l : 0);
    case I_CONV:
        return type_zero(insn->local_type);
    case GL_vtruth:
        return ALL24 & ~1u;
    case GL_vderef:                     /* what is read: as it is in memory */
    case GL_vprefix_indirect: case GL_vpostfix_indirect:
        return type_pointer(insn->in[0].attr.type)
               ? as_type(0, type_deref(insn->in[0].attr.type)) : 0;
    case GL_vapply:
        if (insn->rec->arg[1] && type_size((Type) insn->rec->arg[1]) == 1)
            return type_zero((Type) insn->rec->arg[1]);
        if (type_pointer(insn->in[0].attr.type) || type_pointer(insn->in[1].attr.type))
            return 0;                   /* scaled: nothing carries over */
        switch ((int) insn->rec->arg[0]) {
        case TK_LT: case TK_GT: case TK_LE: case TK_GE: case TK_EQ: case TK_NE:
            return ALL24 & ~1u;
        case TK_AMP:
            return l | r;
        case TK_PIPE: case TK_CARET:
            return l & r;
        case TK_PLUS:
            return (top_zero(l) & top_zero(r)) << 1 & ALL24;
        case TK_SHR:
            if (!is_num(&insn->in[1]) || !type_unsigned(insn->in[0].attr.type))
                return 0;
            k = insn->in[1].attr.val;
            return k >= 24 ? ALL24 : ((l >> k) | (ALL24 << (24 - k))) & ALL24;
        case TK_SHL:
            if (!is_num(&insn->in[1]))
                return 0;
            k = insn->in[1].attr.val;
            return k >= 24 ? ALL24 : ((l << k) | ((1u << k) - 1)) & ALL24;
        case TK_PERCENT:
            if (!is_num(&insn->in[1]) || insn->in[1].attr.val <= 0
                || !(type_unsigned(insn->in[0].attr.type)
                     || (l & 0x800000)))
                return 0;
            return top_zero(~(unsigned) (insn->in[1].attr.val - 1) & ALL24);
        }
        return 0;
    }

    return 0;
}

static void known_bits(void)
{
    int blk, at, phi, pred;
    int *head = malloc(((size_t) nblocks + 1) * sizeof *head);
    int *next = malloc(((size_t) nphis + 1) * sizeof *next);

    known_zero = realloc(known_zero, ((size_t) nvals + 1) * sizeof *known_zero);
    known_done = realloc(known_done, (size_t) nvals + 1);
    if (!known_zero || !known_done || !head || !next)
        acc_error("out of memory for the machine IR");
    memset(known_done, 0, (size_t) nvals + 1);
    for (blk = 0; blk != nblocks; blk++)
        head[blk] = -1;
    for (phi = nphis - 1; phi >= 0; phi--)
        if (phis[phi].live) {
            next[phi] = head[phis[phi].block];
            head[phis[phi].block] = phi;
        }
    for (blk = 0; blk != nblocks; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;

        for (phi = head[blk]; phi >= 0; phi = next[phi]) {
            unsigned kz = ALL24;

            for (pred = 0; pred != preds[blk].count; pred++) {
                int in = phis[phi].in[pred];

                kz &= in >= 0 && known_done[in] ? known_zero[in] : 0;
            }
            known_zero[phis[phi].val] = as_type(kz, vals[phis[phi].val].type);
            known_done[phis[phi].val] = 1;
        }
        for (at = blocks[blk].first; at != end; at++) {
            const Ins *insn = &insns[at];

            if (insn->res < 0 || multi_def[insn->res])
                continue;
            known_zero[insn->res] = as_type(insn_zero(insn), vals[insn->res].type);
            known_done[insn->res] = 1;
        }
    }
    free(head);
    free(next);
}

/* Whether an operand is known to be a byte widened by zeros, or known not
 * to be negative. */
static int known_byte(const Ent *ent)
{
    return (ent_zero(ent) & 0xffff00u) == 0xffff00u;
}

static int known_nonneg(const Ent *ent)
{
    return (ent_zero(ent) & 0x800000u) != 0;
}

static int byte_kind(const Ent *ent)
{
    int v;

    if (is_num(ent)) {
        int c = ent->attr.val;

        return (c >= 0 && c <= 255 ? K_ZEXT : 0) | (c >= -128 && c <= 127 ? K_SEXT : 0);
    }
    if (ent->val < 0)
        return 0;                       /* an address: never a byte */
    v = val_vr[ent->val];
    if (v < 0)
        return 0;
    if (vr[v].width == 1)
        return type_unsigned(vals[ent->val].type) || vals[ent->val].type == TY_BOOL
               ? K_ZEXT : K_SEXT;
    if (vr[v].ext >= 0)
        return vr[v].ext_signed ? K_SEXT : K_ZEXT;
    if (known_byte(ent))
        return K_ZEXT;

    return 0;
}

/* A constant one larger than one compared with, for sel_compare. */
static Ent const_bump;

/* The flags, for a comparison: its condition. */
/* The flags of an operand tested against 0, Z where it is: a byte, or an
 * int that is a byte widened, tested as the byte. */
static void zero_test(const Ent *ent)
{
    int val = ent->val;

    if (val >= 0 && (val_width(val) == 1 || known_byte(ent)
                     || (val_vr[val] >= 0 && vr[val_vr[val]].ext >= 0))) {
        MIns *mi = mi3(M_CMP8I, -1, in_class(operand_vr(ent, 1), C_A), -1);

        mi->imm2 = 0;
        return;
    }
    mi3(M_TST24, -1, in_class(operand_vr(ent, 3), C_HL), -1);
}

static int sel_compare(const Ins *insn, int op)
{
    Type lt = insn->in[0].attr.type, rt = insn->in[1].attr.type;
    int is_signed = !type_unsigned(lt) && !type_unsigned(rt) && !type_pointer(lt)
                    && !type_pointer(rt) && op != TK_EQ && op != TK_NE
                    && !(known_nonneg(&insn->in[0]) && known_nonneg(&insn->in[1]));
    int swap = op == TK_GT || op == TK_LE;
    const Ent *left = &insn->in[swap], *right = &insn->in[!swap];
    int a, b;

    if (swap)
        op = op == TK_GT ? TK_LT : TK_GE;
    /* A constant on the left: to the right, where the byte comparisons
     * and the immediates want it -- c < x as x >= c + 1, c >= x as
     * x < c + 1, equality either way round -- but not for a constant
     * with no next one: 0x7fffff, or 0xffffff held as -1. */
    if (is_num(left) && right->val >= 0
        && left->attr.val != 0x7fffff && left->attr.val != -1) {
        const Ent *t = left;

        if (op == TK_LT || op == TK_GE) {
            Ent *c1 = &const_bump;

            *c1 = *left;
            c1->attr.val = left->attr.val + 1;
            left = right;
            right = c1;
            op = op == TK_LT ? TK_GE : TK_LT;
        } else {
            left = right;
            right = t;
        }
    }

    /* Two bytes widened the same way, or a byte and a constant it can be:
     * cp in A. Ordered, unsigned on the bytes -- but a signed comparison
     * of two widened by their sign, moved by 0x80 first. */
    {
        int lk = byte_kind(left), rk = byte_kind(right), kind = lk & rk;

        if (left->val >= 0 && kind) {
            int bias, value = 0;

            if (kind == (K_ZEXT | K_SEXT))
                kind = K_ZEXT;                  /* two constants: either */
            bias = is_signed && kind == K_SEXT && op != TK_EQ && op != TK_NE ? 0x80 : 0;
            a = in_class(operand_vr(left, 1), C_A);
            if (bias) {
                int t = new_vr(1, C_A);
                MIns *mi = mi3(M_ALU8I, t, a, -1);

                mi->imm = TK_CARET;
                mi->imm2 = bias;
                a = t;
            }
            if (is_num(right)) {
                MIns *mi = mi3(M_CMP8I, -1, a, -1);

                value = right->attr.val;
                mi->imm2 = (value + bias) & 0xff;
            } else if (bias) {
                int t = new_vr(1, C_A), u = new_vr(1, C_R8);
                MIns *mi;

                mi3(M_COPY, u, a, -1);          /* the left, moved, kept */
                mi = mi3(M_ALU8I, t, in_class(operand_vr(right, 1), C_A), -1);
                mi->imm = TK_CARET;
                mi->imm2 = bias;
                b = new_vr(1, C_R8);
                mi3(M_COPY, b, t, -1);
                mi3(M_CMP8, -1, in_class(u, C_A), b);
            } else {
                mi3(M_CMP8, -1, a, operand_vr(right, 1));
            }
            return op == TK_LT ? JP_C : op == TK_GE ? JP_NC : op == TK_EQ ? JP_Z : JP_NZ;
        }
    }

    /* Against zero for equality: kept where it is. */
    if ((op == TK_EQ || op == TK_NE) && is_num(right) && right->attr.val == 0) {
        zero_test(left);
        return op == TK_EQ ? JP_Z : JP_NZ;
    }
    a = in_class(operand_vr(left, 3), C_HL);

    /* Signed against a constant: the left moved by half the range in HL
     * and the constant moved already -- or, against 0, the sign itself. */
    if (is_signed && is_num(right)) {
        MIns *mi = mi3(M_CMP24SI, -1, a, -1);

        mi->imm2 = right->attr.val & 0xffffff;
        mi->kills = 1;
        if (mi->imm2)
            mi->t = new_vr(3, C_DE);
        return op == TK_LT ? JP_C : JP_NC;
    }
    b = in_class(operand_vr(right, 3), is_signed ? C_DE : C_O24);
    if (is_signed) {
        mi3(M_CMP24S, -1, a, b)->kills = 1;
    } else {
        mi3(M_CMP24, -1, a, b)->kills = 1;
    }

    return op == TK_LT ? JP_C : op == TK_GE ? JP_NC : op == TK_EQ ? JP_Z : JP_NZ;
}

/* A branch on the flags, from the SSA block `blk`: taken to `target`, or
 * on to the next block. */
static void sel_branch(int cc, const Ins *branch)
{
    MIns *mi;

    if (!branch->sense)
        cc ^= 0x08;
    mi = mi3(M_BR, -1, -1, -1);
    mi->imm = cc;
    mi->imm2 = branch->target;
}

/* A comparison whose one use is the branch after it: the flags, and the
 * branch on them. */
static int fused_branch(int at)
{
    const Ins *insn = &insns[at], *next;
    int n = at + 1;

    while (n < ninsns && (insns[n].op == GL_vdrop && insns[n].nin == 0))
        n++;
    if (n >= ninsns || insn->res < 0)
        return -1;
    next = &insns[n];
    if (next->op != I_BR || next->nin != 1 || next->in[0].val != insn->res
        || next->target < 0 || next->block != insn->block
        || use_n[insn->res] != 1)
        return -1;

    return n;
}

/* A runtime routine on two ints: HL and BC in, HL out. */
static void sel_helper(const Ins *insn, int which, int left, int right)
{
    int a = in_class(left, C_HL), b = in_class(right, C_BC), d = new_vr(3, C_HL);
    MIns *mi = mi3(M_HELPER, d, a, b);

    mi->imm = which;
    to_val(insn->res, d);
}

/* A pointer's step, scaled: the int times `step`, in HL. */
static int scaled(int v, int step)
{
    int t, o = -1, adds = 0, bit, top;

    if (step == 1)
        return v;
    if (vr[v].remat == M_LDI)           /* a constant: the product, made */
        return const_vr(vr[v].remat_imm * step, 3);

    /* By adds, the constant's bits from the top down: doubled at each,
     * the int itself added where one is set -- two adds for an int's 3 --
     * kept in DE or BC for that. The routine where that is more adds
     * than eight. */
    for (top = 0; (step >> (top + 1)) > 0; top++)
        ;
    for (bit = top - 1; bit >= 0; bit--)
        adds += 1 + ((step >> bit) & 1);
    if (step <= 0 || adds > 8) {
        int u = new_vr(3, C_HL);
        MIns *mi = mi3(M_HELPER, u, in_class(v, C_HL), in_class(const_vr(step, 3), C_BC));

        mi->imm = RT_MUL;
        return u;
    }
    if (step & ((1 << top) - 1))
        o = in_class(v, C_O24);
    t = in_class(v, C_HL);
    for (bit = top - 1; bit >= 0; bit--) {
        int u = new_vr(3, C_HL);

        mi3(M_ADD24, u, t, t);
        t = u;
        if ((step >> bit) & 1) {
            u = new_vr(3, C_HL);
            mi3(M_ADD24, u, t, o);
            t = u;
        }
    }

    return t;
}

/* How &, | or ^ of two bytes widened is itself one: & with one widened
 * by zeros is, by zeros; otherwise as both are. 0: not a byte. */
static int bitwise_kind(int op, int left, int right)
{
    if (!left || !right)
        return 0;
    if (op == TK_AMP && ((left | right) & K_ZEXT) && ((left & K_ZEXT) || (right & K_ZEXT)))
        return K_ZEXT;
    if (left & right & K_ZEXT)
        return K_ZEXT;
    if (left & right & K_SEXT)
        return K_SEXT;

    return 0;
}

/* What a byte of &, | or ^ with `c` costs made in place: nothing where
 * it changes nothing, two where it is ld r, 0 or ld r, 0xff, four
 * through A. */
static int byte_op_cost(int op, int c)
{
    if ((op == TK_AMP && c == 0xff) || (op != TK_AMP && c == 0))
        return 0;
    if ((op == TK_AMP && c == 0) || (op == TK_PIPE && c == 0xff))
        return 2;

    return 4;
}

/* &, | and ^ of an int and a constant, without the routine where the
 * constant allows: every bit kept, nothing made; a mask of the low byte,
 * the byte and'ed and widened; the top byte left as it is, the two below
 * worked on in place -- where that is no dearer than the call, ld bc, n
 * and call, eight bytes. A byte answered is one to widen by zeros. -1
 * where none of them is. */
static int sel_bitwise_const(const Ins *insn, int op)
{
    const Ent *x = &insn->in[0], *k = &insn->in[1];
    int c, cost, d, a;
    MIns *mi;

    if (is_num(x)) {
        const Ent *t = x;

        x = k;
        k = t;
    }
    if (!is_num(k) || x->val < 0)
        return -1;
    c = k->attr.val & 0xffffff;
    if (op == TK_AMP)
        c |= ent_zero(x);               /* what is 0 already needs no mask */
    if ((op == TK_AMP && c == 0xffffff) || (op != TK_AMP && c == 0))
        return operand_vr(x, 3);
    if (op == TK_AMP && c <= 0xff) {
        a = in_class(operand_vr(x, 1), C_A);
        d = new_vr(1, C_A);
        mi = mi3(M_ALU8I, d, a, -1);
        mi->imm = TK_AMP;
        mi->imm2 = c;
        return d;                       /* a byte: widened by zeros */
    }
    if ((c >> 16) != (op == TK_AMP ? 0xff : 0))
        return -1;
    cost = byte_op_cost(op, c & 0xff) + byte_op_cost(op, (c >> 8) & 0xff);
    if (cost > 8)
        return -1;
    a = in_class(operand_vr(x, 3), C_P24);
    d = new_vr(3, C_P24);
    mi = mi3(M_BYTES24, d, a, -1);
    mi->imm = op;
    mi->imm2 = c;
    if (byte_op_cost(op, c & 0xff) == 4 || byte_op_cost(op, (c >> 8) & 0xff) == 4)
        mi->t = new_vr(1, C_A);

    return d;
}

static void sel_apply(const Ins *insn, int at)
{
    int op = (int) insn->rec->arg[0], n;
    Type narrow = (Type) insn->rec->arg[1];
    Type lt = insn->in[0].attr.type, rt = insn->in[1].attr.type;
    int is_unsigned = type_unsigned(lt) || type_unsigned(rt);
    int a, b, d, step;

    switch (op) {
    case TK_LT: case TK_GT: case TK_LE: case TK_GE: case TK_EQ: case TK_NE: {
        int cc = sel_compare(insn, op);

        n = fused_branch(at);
        if (n >= 0) {
            sel_branch(cc, &insns[n]);
            skip_to = n;
            return;
        }
        if (insn->res >= 0 && val_vr[insn->res] >= 0) {
            MIns *mi;

            d = new_vr(width_of(vals[insn->res].type), width_of(vals[insn->res].type) == 1 ? C_R8 : C_P24);
            mi = mi3(M_BOOL, d, -1, -1);
            mi->imm = cc;
            to_val(insn->res, d);
        }
        return;
    }
    }

    /* A byte's operator, narrowed to the byte: in A. */
    if (narrow && type_size(narrow) == 1
        && (op == TK_PLUS || op == TK_MINUS || op == TK_AMP || op == TK_PIPE
            || op == TK_CARET)) {
        a = in_class(operand_vr(&insn->in[0], 1), C_A);
        d = new_vr(1, C_A);
        if (is_num(&insn->in[1])) {
            MIns *mi = mi3(M_ALU8I, d, a, -1);

            mi->imm = op;
            mi->imm2 = insn->in[1].attr.val & 0xff;
        } else {
            MIns *mi = mi3(M_ALU8, d, a, operand_vr(&insn->in[1], 1));

            mi->imm = op;
        }
        to_val(insn->res, d);
        return;
    }

    /* &, | and ^ of two bytes, or of a byte and a constant one could be:
     * in A, the answer widened as unsigned where both are -- the bytes
     * above being 0 on both sides. */
    if ((op == TK_AMP || op == TK_PIPE || op == TK_CARET)
        && (insn->in[0].val >= 0 || insn->in[1].val >= 0)
        && bitwise_kind(op, byte_kind(&insn->in[0]), byte_kind(&insn->in[1]))) {
        const Ent *l = &insn->in[0], *r = &insn->in[1];
        int kind = bitwise_kind(op, byte_kind(l), byte_kind(r));
        MIns *mi;

        if (is_num(l)) {
            const Ent *t = l;

            l = r;
            r = t;
        }
        a = in_class(operand_vr(l, 1), C_A);
        d = new_vr(1, C_A);
        if (is_num(r)) {
            mi = mi3(M_ALU8I, d, a, -1);
            mi->imm2 = r->attr.val & 0xff;
        } else {
            mi = mi3(M_ALU8, d, a, operand_vr(r, 1));
        }
        mi->imm = op;
        to_val_as(insn->res, d, kind == K_ZEXT ? TY_UCHAR : TY_CHAR);
        return;
    }

    switch (op) {
    case TK_PLUS: case TK_MINUS:
        step = 1;
        if (type_pointer(lt) && type_pointer(rt)) {
            /* A pointer from a pointer: the difference, divided. */
            a = in_class(operand_vr(&insn->in[0], 3), C_HL);
            b = in_class(operand_vr(&insn->in[1], 3), C_O24);
            d = new_vr(3, C_HL);
            mi3(M_SUB24, d, a, b);
            step = type_step(lt, insn->in[0].attr.ext);
            if (step != 1) {
                int q = new_vr(3, C_HL);
                MIns *mi = mi3(M_HELPER, q, d, in_class(const_vr(step, 3), C_BC));

                mi->imm = RT_DIVS;
                d = q;
            }
            break;
        }
        if (type_pointer(lt)) {
            step = type_step(lt, insn->in[0].attr.ext);
            a = operand_vr(&insn->in[0], 3);
            b = scaled(operand_vr(&insn->in[1], 3), step);
        } else if (type_pointer(rt)) {
            step = type_step(rt, insn->in[1].attr.ext);
            a = scaled(operand_vr(&insn->in[0], 3), step);
            b = operand_vr(&insn->in[1], 3);
        } else {
            a = operand_vr(&insn->in[0], 3);
            b = -1;
        }
        /* A small constant: inc or dec. */
        if (is_num(&insn->in[1]) && !type_pointer(rt)) {
            int k = insn->in[1].attr.val * step;

            if (op == TK_MINUS)
                k = -k;
            if (k >= -4 && k <= 4) {
                MIns *mi;

                d = new_vr(3, C_R24);
                mi = mi3(M_STEP24, d, a, -1);
                mi->imm = k;
                break;
            }
        }
        if (b < 0)
            b = operand_vr(&insn->in[1], 3);
        /* The left in HL: the right instead where it is made there --
         * a scaled index -- or the left is a constant, or the right was
         * made later and goes no further, so that the one made now stays
         * in HL and the older one is the other side. */
        if (op == TK_PLUS && (insn->in[0].val == S_CONST || vr[b].cls == C_HL
                              || (insn->in[0].val >= 0 && insn->in[1].val >= 0
                                  && use_n[insn->in[1].val] == 1
                                  && vals[insn->in[1].val].def > vals[insn->in[0].val].def))) {
            int swap = a;

            a = b;
            b = swap;
        }
        a = in_class(a, C_HL);
        b = in_class(b, op == TK_PLUS ? C_P24 : C_O24);
        d = new_vr(3, C_HL);
        mi3(op == TK_PLUS ? M_ADD24 : M_SUB24, d, a, b);
        break;
    case TK_STAR:
        sel_helper(insn, RT_MUL, operand_vr(&insn->in[0], 3), operand_vr(&insn->in[1], 3));
        return;
    case TK_SLASH:
        sel_helper(insn, is_unsigned ? RT_DIVU : RT_DIVS,
                   operand_vr(&insn->in[0], 3), operand_vr(&insn->in[1], 3));
        return;
    case TK_PERCENT:
        sel_helper(insn, is_unsigned ? RT_REMU : RT_REMS,
                   operand_vr(&insn->in[0], 3), operand_vr(&insn->in[1], 3));
        return;
    case TK_AMP: case TK_PIPE: case TK_CARET:
        d = sel_bitwise_const(insn, op);
        if (d >= 0) {
            if (vr[d].width == 1) {
                to_val_as(insn->res, d, TY_UCHAR);
                return;
            }
            break;
        }
        sel_helper(insn, op == TK_AMP ? RT_AND : op == TK_PIPE ? RT_OR : RT_XOR,
                   operand_vr(&insn->in[0], 3), operand_vr(&insn->in[1], 3));
        return;
    case TK_SHL:
        /* By a small constant: adds. */
        if (is_num(&insn->in[1]) && insn->in[1].attr.val >= 1
            && insn->in[1].attr.val <= 3) {
            int k;

            a = in_class(operand_vr(&insn->in[0], 3), C_HL);
            for (k = 0; k != insn->in[1].attr.val; k++) {
                d = new_vr(3, C_HL);
                mi3(M_ADD24, d, a, a);
                a = d;
            }
            break;
        }
        sel_helper(insn, RT_SHL, operand_vr(&insn->in[0], 3), operand_vr(&insn->in[1], 3));
        return;
    case TK_SHR:
        sel_helper(insn, type_unsigned(lt) ? RT_SHRU : RT_SHRS,
                   operand_vr(&insn->in[0], 3), operand_vr(&insn->in[1], 3));
        return;
    default:
        sel_fail = "internal: an operator mir_ok let through";
        return;
    }

    /* The answer, narrowed where the operator was. */
    if ((narrow && type_size(narrow) == 1)
        || (insn->res >= 0 && val_vr[insn->res] >= 0
            && vr[val_vr[insn->res]].width != vr[d].width)) {
        int t = new_vr(1, C_R8);

        mi3(M_TRUNC, t, in_class(d, C_P24), -1);
        d = t;
    }
    if (insn->res >= 0 && val_vr[insn->res] >= 0)
        to_val(insn->res, d);
}

/* A conversion of `ent` to `to`, into the value `res`. */
static void sel_convert(const Ent *ent, Type to, int res)
{
    int w = width_of(to), d;

    if (res < 0 || val_vr[res] < 0)
        return;
    if (to == TY_BOOL) {
        MIns *mi;

        zero_test(ent);
        d = new_vr(1, C_R8);
        mi = mi3(M_BOOL, d, -1, -1);
        mi->imm = JP_NZ;
        to_val(res, d);
        return;
    }
    if (is_num(ent)) {
        int value = ent->attr.val;

        if (w == 1)
            value = type_unsigned(to) ? value & 0xff : (signed char) (value & 0xff);
        to_val_as(res, const_vr(value, w), to);
        return;
    }
    d = operand_vr(ent, w);
    /* A byte to another byte keeps its bits: a char made unsigned is read
     * as unsigned from here on -- widened by the type it was made, where
     * the value is held as an int. */
    to_val_as(res, d, to);
}

/* The address a load or a store reaches: a pointer's register and an
 * offset folded in -- a member's -- or a global's. */
typedef struct {
    int base;                   /* a virtual register, or -1 for a global */
    int sym, off;
    int kind;                   /* VAL_ADDR or VAL_BSS: off is the address
                                 * constant itself, the link moving it */
} Addr;

static int *member_base, *member_off;   /* by value: a member's pointer */
static int *global_of;                  /* by value: a global's address */
static int *addrc_of;                   /* by value: a constant's kind, the
                                         * address in member_off, or 0 */

static Addr address_of(const Ent *ent)
{
    Addr addr;
    int val = ent->val;

    addr.base = -1;
    addr.sym = -1;
    addr.off = 0;
    addr.kind = 0;
    if (is_addr(ent)) {
        addr.kind = ent->attr.kind;
        addr.off = ent->attr.val;
        return addr;
    }
    if (val >= 0 && addrc_of[val]) {
        addr.kind = addrc_of[val];
        addr.off = member_off[val];
        return addr;
    }
    if (val >= 0 && global_of[val] >= 0) {
        addr.sym = global_of[val];
        addr.off = member_off[val];
        return addr;
    }
    if (val >= 0 && member_base[val] >= 0) {
        addr.off = member_off[val];
        val = member_base[val];
        if (global_of[val] >= 0) {
            addr.sym = global_of[val];
            addr.off += member_off[val];
            return addr;
        }
    }
    addr.base = val >= 0 ? val_vr[val] : operand_vr(ent, 3);
    if (addr.base < 0) {
        sel_fail = "internal: an address with no register";
        addr.base = const_vr(0, 3);
    }

    return addr;
}

/* A folded address made whole, where it is wanted as a value: the
 * global's, or the pointer and the member's offset added. */
static int addr_vr(const Ent *ent)
{
    Addr addr = address_of(ent);
    int d;
    MIns *mi;

    if (addr.kind)
        return addrc_vr(addr.off, addr.kind);
    if (addr.sym >= 0) {
        d = new_vr(3, C_R24);
        mi = mi3(M_LDSYM, d, -1, -1);
        mi->sym = addr.sym;
        mi->imm = addr.off;
        vr[d].remat = M_LDSYM;
        vr[d].remat_sym = addr.sym;
        vr[d].remat_imm = addr.off;
        return d;
    }
    if (!addr.off)
        return addr.base;
    d = new_vr(3, C_HL);
    mi3(M_ADD24, d, in_class(addr.base, C_HL),
        in_class(const_vr(addr.off, 3), C_O24));

    return d;
}


/* A pointer plus or minus a constant, the bytes it moves by: or INT_MIN. */
static int const_step(const Ins *insn)
{
    int op = (int) insn->rec->arg[0];
    long long k;
    Type lt = insn->in[0].attr.type;

    if (insn->op != GL_vapply || (op != TK_PLUS && op != TK_MINUS)
        || insn->in[0].val < 0 || !type_pointer(lt) || !is_num(&insn->in[1])
        || !mir_operand_ok(&insn->in[0]))
        return INT_MIN;
    k = (long long) insn->in[1].attr.val * type_step(lt, insn->in[0].attr.ext);
    if (k < -128 || k > 128)
        return INT_MIN;

    return (int) (op == TK_MINUS ? -k : k);
}

/* Whether an instruction reads or writes through its first operand: its
 * address folded into it, (nn) or (iy+d). */
static int through(int op)
{
    return op == GL_vderef || op == GL_vstore_indirect
           || op == GL_vprefix_indirect || op == GL_vpostfix_indirect;
}

/* The one instruction that uses `val`, or -1. */
static int sole_user(int val)
{
    return use_n[val] == 1 ? user_of[val] : -1;
}

static void sel_load(const Ins *insn)
{
    Type read = type_deref(insn->in[0].attr.type);
    int w, d;
    Addr addr;
    MIns *mi;

    if (insn->res < 0 || val_vr[insn->res] < 0)
        return;
    if (type_is_struct(read) || type_is_array(read)) {
        to_val(insn->res, operand_vr(&insn->in[0], 3));
        return;
    }
    w = width_of(read);
    addr = address_of(&insn->in[0]);
    if (addr.sym >= 0 || addr.kind) {
        d = new_vr(w, w == 1 ? C_A : C_R24);
        mi = mi3(M_LDG, d, -1, -1);
        mi->sym = addr.sym;
        mi->imm = addr.off;
        mi->imm2 = addr.kind;
        mi->width = w;
        to_val_as(insn->res, d, read);
        return;
    }
    /* A byte at the pointer itself: into A, through any pair -- ld a, (bc)
     * and ld a, (de) as well as (hl). */
    if (w == 1 && !addr.off) {
        int base = addr.base, t = new_vr(1, C_A);

        /* Read where it is: a copy of a phi's could not share its pair.
         * Into A, and copied from there to where it goes: through HL or
         * IY, the two made one, ld r, (hl) (mir_emit). */
        if (vr[base].cls & ~C_R24)
            base = in_class(base, C_R24);
        mi = mi3(M_LDP, t, base, -1);
        mi->width = 1;
        d = new_vr(1, C_R8);
        mi3(M_COPY, d, t, -1);
    } else {
        d = new_vr(w, w == 1 ? C_R8 : C_R24);
        mi = mi3(M_LDP, d, in_class(addr.base, addr.off ? C_IY : PB(P_HL) | C_IY), -1);
        mi->imm = addr.off;
        mi->width = w;
    }
    to_val_as(insn->res, d, read);
}

static void sel_store(const Ins *insn)
{
    Type to = type_deref(insn->in[0].attr.type);
    int w = width_of(to), v;
    Addr addr = address_of(&insn->in[0]);
    MIns *mi;

    if (is_num(&insn->in[1]) && w == 1) {
        int value = insn->in[1].attr.val;

        if (to == TY_BOOL)
            value = value != 0;
        if (addr.sym >= 0 || addr.kind) {
            v = in_class(const_vr(value, 1), C_A);
            mi = mi3(M_STG, -1, v, -1);
            mi->sym = addr.sym;
            mi->imm = addr.off;
            mi->imm2 = addr.kind;
            mi->width = 1;
        } else {
            mi = mi3(M_STPI, -1, in_class(addr.base, addr.off ? C_IY : PB(P_HL) | C_IY), -1);
            mi->imm = addr.off;
            mi->imm2 = value & 0xff;
            mi->width = 1;
        }
        if (insn->res >= 0 && val_vr[insn->res] >= 0)
            to_val(insn->res, const_vr(value, vr[val_vr[insn->res]].width));
        return;
    }
    if (to == TY_BOOL) {
        zero_test(&insn->in[1]);
        v = new_vr(1, C_R8);
        mi = mi3(M_BOOL, v, -1, -1);
        mi->imm = JP_NZ;
    } else {
        v = operand_vr(&insn->in[1], w);
    }
    if (addr.sym >= 0 || addr.kind) {
        mi = mi3(M_STG, -1, in_class(v, w == 1 ? C_A : C_R24), -1);
        mi->sym = addr.sym;
        mi->imm = addr.off;
        mi->imm2 = addr.kind;
        mi->width = w;
    } else if (w == 1 && !addr.off) {
        int base = addr.base;

        /* ld (bc), a and (de), a: the byte copied into A first -- which
         * through HL or IY is no copy, ld (hl), r (mir_emit). */
        if (vr[base].cls & ~C_R24)
            base = in_class(base, C_R24);
        mi = mi3(M_STP, -1, base, in_class(v, C_A));
        mi->width = 1;
    } else {
        int base = in_class(addr.base, addr.off ? C_IY : PB(P_HL) | C_IY);

        mi = mi3(M_STP, -1, base, in_class(v, w == 1 ? C_R8 : C_R24));
        mi->imm = addr.off;
        mi->width = w;
    }
    /* The assignment's value is what was stored, as its type has it. */
    to_val_as(insn->res, v, to);
}

/* A byte read through a pointer in HL or IY, (hl) or (iy+d). */
static int byte_at(int base, int off)
{
    int d = new_vr(1, C_R8);
    MIns *mi = mi3(M_LDP, d, base, -1);

    mi->imm = off;
    mi->width = 1;

    return d;
}

/* ++ and -- through a pointer. A byte is stepped where it is -- inc (hl),
 * dec (iy+d), a global's through its address -- and read before or after,
 * where what the expression is is wanted. Anything wider is read, stepped
 * and written back, through (iy+d), (hl) or (nn) as its reads and writes
 * are. */
static void sel_step_through(const Ins *insn)
{
    Type to = type_deref(insn->in[0].attr.type);
    int post = insn->op == GL_vpostfix_indirect;
    int used = insn->res >= 0 && val_vr[insn->res] >= 0;
    int step = type_pointer(to) ? type_step(to, insn->in[0].attr.ext) : 1;
    int global, base = -1, old, now;
    Addr addr = address_of(&insn->in[0]);
    MIns *mi;

    if ((int) insn->rec->arg[0] == TK_MINUS)
        step = -step;
    global = addr.sym >= 0 || addr.kind;
    if (width_of(to) == 1) {
        if (global) {
            base = addr_vr(&insn->in[0]);
            addr.off = 0;
        } else {
            base = addr.base;
        }
        base = in_class(base, addr.off ? C_IY : PB(P_HL) | C_IY);
        old = used && post ? byte_at(base, addr.off) : -1;
        mi = mi3(M_STEPP, -1, base, -1);
        mi->imm = addr.off;
        mi->imm2 = step;
        mi->width = 1;
        if (used && !post)
            old = byte_at(base, addr.off);
        if (used)
            to_val_as(insn->res, old, to);
        return;
    }

    old = new_vr(3, C_R24);
    if (global) {
        mi = mi3(M_LDG, old, -1, -1);
        mi->sym = addr.sym;
        mi->imm2 = addr.kind;
    } else {
        base = in_class(addr.base, addr.off ? C_IY : PB(P_HL) | C_IY);
        mi = mi3(M_LDP, old, base, -1);
    }
    mi->imm = addr.off;
    mi->width = 3;
    if (step >= -4 && step <= 4) {
        now = new_vr(3, C_R24);
        mi = mi3(M_STEP24, now, old, -1);
        mi->imm = step;
    } else {
        now = new_vr(3, C_HL);
        mi3(M_ADD24, now, in_class(old, C_HL), in_class(const_vr(step, 3), C_O24));
    }
    if (global) {
        mi = mi3(M_STG, -1, in_class(now, C_R24), -1);
        mi->sym = addr.sym;
        mi->imm2 = addr.kind;
    } else {
        mi = mi3(M_STP, -1, base, in_class(now, C_R24));
    }
    mi->imm = addr.off;
    mi->width = 3;
    if (used)
        to_val(insn->res, post ? old : now);
}

/* A call: each argument as wide as a slot, pushed last first so that
 * the first is lowest, under the pairs live across the call. */
static void sel_call(const Ins *insn)
{
    const Sym *callee = sym_at((int) insn->rec->arg[0]);
    int arg, d = -1, *args = malloc(((size_t) insn->nin + 1) * sizeof *args);
    MIns *mi;

    if (!args)
        acc_error("out of memory for the machine IR");
    for (arg = 0; arg != insn->nin; arg++)
        args[arg] = operand_vr(&insn->in[arg], 3);
    mi3(M_SAVE, -1, -1, -1);
    for (arg = insn->nin - 1; arg >= 0; arg--)
        mi3(M_PUSH, -1, in_class(args[arg], C_R24), -1);
    free(args);
    if (callee->type != TY_VOID && insn->res >= 0 && val_vr[insn->res] >= 0)
        d = width_of(callee->type) == 1 ? new_vr(1, C_A) : new_vr(3, C_HL);
    mi = mi3(M_CALL, d, -1, -1);
    mi->sym = (int) insn->rec->arg[0];
    mi->imm = insn->nin;
    if (d >= 0)
        to_val_as(insn->res, d, callee->type == TY_BOOL ? TY_UCHAR : callee->type);
}

/* Whether `v`, an operand of `type` as an int, is what the function
 * answers already, gen_return having nothing to convert: an int or a
 * pointer as another, or the function's byte widened as it widens it. */
static int answer_made(int v, Type type)
{
    Type to = return_type;

    if (!mir_type(to) || !mir_type(type))
        return 0;
    if (to == TY_BOOL)
        return type == TY_BOOL && vr[v].ext >= 0 && !vr[v].ext_signed;
    if (width_of(to) == 3)
        return width_of(type) == 3;

    return vr[v].ext >= 0 && vr[v].ext_signed == !type_unsigned(to);
}

static void sel_insn(const Ins *insn, int at)
{
    int op = insn->op, d, a;
    MIns *mi;

    switch (op) {
    case I_FRAME: case GL_vdrop: case GL_gen_stmt_end: case GL_gen_value_end:
        return;
    case GL_vpush_const:                /* read where it is used, or a phi's */
        if (insn->res >= 0 && val_vr[insn->res] >= 0)
            to_val(insn->res, const_vr((int) insn->rec->arg[0],
                                       vr[val_vr[insn->res]].width));
        return;
    case GL_vpush_global_addr:
        if (insn->res >= 0 && val_vr[insn->res] >= 0) {
            d = new_vr(3, C_R24);
            mi = mi3(M_LDSYM, d, -1, -1);
            mi->sym = (int) insn->rec->arg[0];
            vr[d].remat = M_LDSYM;
            vr[d].remat_sym = mi->sym;
            to_val(insn->res, d);
        }
        return;
    case GL_vmember:
        if (insn->res < 0 || val_vr[insn->res] < 0)
            return;
        {
            Addr addr = address_of(&insn->in[0]);

            if (addr.kind) {
                to_val(insn->res, addrc_vr(addr.off + (int) insn->rec->arg[0],
                                           addr.kind));
                return;
            }
            if (addr.sym >= 0) {
                d = new_vr(3, C_R24);
                mi = mi3(M_LDSYM, d, -1, -1);
                mi->sym = addr.sym;
                mi->imm = addr.off + (int) insn->rec->arg[0];
                vr[d].remat = M_LDSYM;
                vr[d].remat_sym = mi->sym;
                vr[d].remat_imm = mi->imm;
                to_val(insn->res, d);
                return;
            }
            a = in_class(addr.base, C_HL);
            d = new_vr(3, C_HL);
            mi3(M_ADD24, d, a, in_class(const_vr(addr.off + (int) insn->rec->arg[0], 3), C_O24));
            to_val(insn->res, d);
        }
        return;
    case I_SET:
        if (insn->target >= 0 && val_vr[insn->target] >= 0)
            sel_convert(&insn->in[0], vals[insn->target].type, insn->target);
        return;
    case I_CONV:
        sel_convert(&insn->in[0], insn->local_type, insn->res);
        return;
    case GL_vconvert: case GL_vcast:
        sel_convert(&insn->in[0], (Type) insn->rec->arg[0], insn->res);
        return;
    case I_STEP: {
        int step = type_pointer(insn->local_type)
                   ? type_step(insn->local_type, insn->in[0].attr.ext) : 1;
        int w = width_of(insn->local_type);

        if (insn->res < 0 || val_vr[insn->res] < 0)
            return;
        if (insn->step_op == TK_MINUS)
            step = -step;
        if (w == 1) {
            a = in_class(operand_vr(&insn->in[0], 1), C_A);
            d = new_vr(1, C_A);
            mi = mi3(M_ALU8I, d, a, -1);
            mi->imm = TK_PLUS;
            mi->imm2 = step & 0xff;
        } else if (step >= -4 && step <= 4) {
            d = new_vr(3, C_R24);
            mi = mi3(M_STEP24, d, operand_vr(&insn->in[0], 3), -1);
            mi->imm = step;
        } else {
            d = new_vr(3, C_HL);
            mi3(M_ADD24, d, in_class(operand_vr(&insn->in[0], 3), C_HL),
                in_class(const_vr(step, 3), C_O24));
        }
        to_val(insn->res, d);
        return;
    }
    case GL_vneg: case GL_vnot:
        if (insn->res < 0 || val_vr[insn->res] < 0)
            return;
        a = in_class(operand_vr(&insn->in[0], 3), C_HL);
        d = new_vr(3, C_HL);
        mi = mi3(op == GL_vneg ? M_NEG24 : M_NOT24, d, a, -1);
        mi->t = new_vr(3, C_DE);                /* clobbered */
        if (vr[val_vr[insn->res]].width == 1) {
            int t = new_vr(1, C_R8);

            mi3(M_TRUNC, t, in_class(d, C_P24), -1);
            d = t;
        }
        to_val(insn->res, d);
        return;
    case GL_vtruth: {
        int cc = (int) insn->rec->arg[0] == TK_EQ ? JP_Z : JP_NZ, n;

        zero_test(&insn->in[0]);
        n = fused_branch(at);
        if (n >= 0) {
            sel_branch(cc, &insns[n]);
            skip_to = n;
            return;
        }
        if (insn->res >= 0 && val_vr[insn->res] >= 0) {
            int w = vr[val_vr[insn->res]].width;

            d = new_vr(w, w == 1 ? C_R8 : C_P24);
            mi = mi3(M_BOOL, d, -1, -1);
            mi->imm = cc;
            to_val(insn->res, d);
        }
        return;
    }
    case GL_vderef:
        sel_load(insn);
        return;
    case GL_vstore_indirect:
        sel_store(insn);
        return;
    case GL_vprefix_indirect: case GL_vpostfix_indirect:
        sel_step_through(insn);
        return;
    case GL_vapply:
        if (insn->res >= 0 && member_base[insn->res] >= 0)
            return;                     /* in its reader's displacement */
        sel_apply(insn, at);
        return;
    case GL_gen_call:
        sel_call(insn);
        return;
    case GL_gen_data:
        mi = mi3(M_DATA, -1, -1, -1);
        mi->ssa = at;
        return;
    case I_BR:
        if (insn->target < 0)
            return;
        /* A constant decides it -- an address too, which is never 0. */
        if (is_num(&insn->in[0]) || is_addr(&insn->in[0])) {
            if ((is_addr(&insn->in[0]) || insn->in[0].attr.val != 0) == insn->sense) {
                mi = mi3(M_JMP, -1, -1, -1);
                mi->imm2 = insn->target;
            }
            return;
        }
        zero_test(&insn->in[0]);
        sel_branch(JP_NZ, insn);
        return;
    case I_JMP:
        if (insn->target >= 0) {
            mi = mi3(M_JMP, -1, -1, -1);
            mi->imm2 = insn->target;
        }
        return;
    case GL_gen_return: {
        int v = -1;

        /* The answer in HL, widened as its own type, as the leaf backend
         * hands it to gen_return -- or a constant as the constant, which
         * gen_return knows: a _Bool's is made as it is, not tested, and a
         * return of one made before is a jump back to it. */
        if (insn->nin && is_num(&insn->in[0])) {
            mi = mi3(M_RET, -1, -1, -1);
            mi->imm2 = 1;
            mi->ssa = at;
            mi->type = insn->in[0].attr.type;
            return;
        }
        if (insn->nin)
            v = operand_vr(&insn->in[0], 3);
        mi = mi3(M_RET, -1, v >= 0 ? in_class(v, C_HL) : -1, -1);
        mi->ssa = at;
        mi->type = insn->nin ? insn->in[0].attr.type : TY_VOID;
        mi->imm = v >= 0 && answer_made(v, mi->type);
        return;
    }
    }
    sel_fail = "internal: an instruction mir_ok let through";
}

/* ------------------------------------------------------------------ */
/* the form into machine blocks                                        */

/* Each SSA value its register; the members and the globals folded into
 * their one read or write; the parameters loaded where the function
 * begins. */
static int setup(void)
{
    int val, at, local;

    val_vr = realloc(val_vr, ((size_t) nvals + 1) * sizeof *val_vr);
    member_base = realloc(member_base, ((size_t) nvals + 1) * sizeof *member_base);
    member_off = realloc(member_off, ((size_t) nvals + 1) * sizeof *member_off);
    global_of = realloc(global_of, ((size_t) nvals + 1) * sizeof *global_of);
    addrc_of = realloc(addrc_of, ((size_t) nvals + 1) * sizeof *addrc_of);
    ssa_mb = realloc(ssa_mb, ((size_t) nblocks + 1) * sizeof *ssa_mb);
    if (!val_vr || !member_base || !member_off || !global_of || !addrc_of || !ssa_mb)
        acc_error("out of memory for the machine IR");
    nvr = nmb = npc = 0;
    for (val = 0; val != nvals; val++) {
        val_vr[val] = member_base[val] = global_of[val] = -1;
        member_off[val] = addrc_of[val] = 0;
    }

    /* What is made in more than one place: a phi, and what a ?: sets. */
    multi_def = realloc(multi_def, (size_t) nvals + 1);
    if (!multi_def)
        acc_error("out of memory for the machine IR");
    memset(multi_def, 0, (size_t) nvals + 1);
    for (at = 0; at != nphis; at++)
        multi_def[phis[at].val] = 1;
    for (at = 0; at != ninsns; at++)
        if (insns[at].op == I_SET && insns[at].target >= 0)
            multi_def[insns[at].target] = 1;

    /* What is read at all, by an instruction or a phi, as live_ranges says
     * it -- which has not run yet -- and how often, and by what. */
    use_n = realloc(use_n, ((size_t) nvals + 1) * sizeof *use_n);
    user_of = realloc(user_of, ((size_t) nvals + 1) * sizeof *user_of);
    if (!use_n || !user_of)
        acc_error("out of memory for the machine IR");
    for (val = 0; val != nvals; val++) {
        use_n[val] = 0;
        user_of[val] = -1;
    }
    for (at = 0; at != ninsns; at++) {
        int operand;

        for (operand = 0; operand != insns[at].nin; operand++) {
            int u = insns[at].in[operand].val;

            if (u < 0)
                continue;
            vals[u].used = 1;
            if (user_of[u] != at)
                use_n[u]++;
            user_of[u] = at;
        }
    }
    for (local = 0; local != nphis; local++)
        if (phis[local].live) {
            int pred;

            for (pred = 0; pred != preds[phis[local].block].count; pred++)
                if (phis[local].in[pred] >= 0) {
                    vals[phis[local].in[pred]].used = 1;
                    use_n[phis[local].in[pred]] += 2;   /* never "once" */
                }
        }

    /* A global's address, or a member's -- of a pointer, or of a global --
     * read or written in one place: (nn), or (iy+d), there. */
    for (at = 1; at != ninsns; at++) {
        const Ins *insn = &insns[at];
        int res = insn->res, user, off;

        if (res < 0 || !vals[res].used)
            continue;
        user = sole_user(res);
        if (insn->op == GL_vpush_global_addr) {
            global_of[res] = (int) insn->rec->arg[0];
            continue;
        }
        if (user >= 0 && (off = const_step(insn)) != INT_MIN
            && insns[user].in[0].val == res
            && (through(insns[user].op) || insns[user].op == GL_vmember)) {
            /* A pointer and a constant, read or written through or a
             * member's pointer: the constant in the displacement. */
            if (off >= -128 && off + 2 <= 127) {
                member_base[res] = insn->in[0].val;
                member_off[res] = off;
            }
            continue;
        }
        if (insn->op != GL_vmember || user < 0 || !through(insns[user].op)
            || insns[user].in[0].val != res)
            continue;
        if (is_addr(&insn->in[0])) {
            addrc_of[res] = insn->in[0].attr.kind;
            member_off[res] = insn->in[0].attr.val + (int) insn->rec->arg[0];
            continue;
        }
        if (insn->in[0].val < 0)
            continue;
        off = (int) insn->rec->arg[0];
        if (global_of[insn->in[0].val] >= 0) {
            global_of[res] = global_of[insn->in[0].val];
            member_off[res] = member_off[insn->in[0].val] + off;
        } else if (member_base[insn->in[0].val] >= 0) {
            off += member_off[insn->in[0].val];
            if (off >= -128 && off + 2 <= 127) {
                member_base[res] = member_base[insn->in[0].val];
                member_off[res] = off;
            }
        } else if (off >= -128 && off + 2 <= 127) {
            member_base[res] = insn->in[0].val;
            member_off[res] = off;
        }
    }

    for (val = 0; val != nvals; val++) {
        int w;

        if (!vals[val].used || !mir_type(vals[val].type))
            continue;
        if (member_base[val] >= 0 || addrc_of[val])
            continue;                   /* folded into its read or write */
        if (global_of[val] >= 0 && sole_user(val) >= 0
            && (through(insns[sole_user(val)].op)
                || (insns[sole_user(val)].op == GL_vmember
                    && global_of[insns[sole_user(val)].res] >= 0))
            && insns[sole_user(val)].in[0].val == val)
            continue;
        w = width_of(vals[val].type);
        val_vr[val] = new_vr(w, w == 1 ? C_R8 : C_R24);
    }

    for (at = 0; at != nblocks; at++)
        ssa_mb[at] = new_mb(at);

    /* The parameters, each from its slot where the function begins: kept
     * in a register from there, or read from the slot again where the
     * allocator would rather -- never stored, the slot being its own. */
    cur = ssa_mb[0];
    for (local = 0; local != nlocals; local++)
        if (locals[local].ok && locals[local].is_param) {
            int v = locals[local].entry_val;
            MIns *mi;

            if (v < 0 || val_vr[v] < 0)
                continue;
            mi = mi3(M_LDF, val_vr[v], -1, -1);
            mi->imm = inline_moved(locals[local].offset);
            mi->width = vr[val_vr[v]].width;
            vr[val_vr[v]].param = mi->imm;
        }

    return 1;
}

/* The machine blocks' successors, from their last instructions: a branch
 * on to its target and the next block; a jump to its; a return nowhere;
 * anything else into the next block. Branch and jump targets, SSA blocks
 * when selected, are machine blocks from here. */
static void link_blocks(void)
{
    int blk;

    for (blk = 0; blk != nblocks; blk++) {
        MBlock *b = &mb[ssa_mb[blk]];
        int next = blk + 1 < nblocks ? ssa_mb[blk + 1] : -1;

        b->nsucc = 0;
        if (rpo_num[blk] < 0 && blk)
            continue;
        if (b->n && b->ins[b->n - 1].op == M_RET)
            continue;
        if (b->n && b->ins[b->n - 1].op == M_JMP) {
            b->ins[b->n - 1].imm2 = ssa_mb[b->ins[b->n - 1].imm2];
            b->succ[b->nsucc++] = b->ins[b->n - 1].imm2;
            continue;
        }
        if (b->n && b->ins[b->n - 1].op == M_BR) {
            b->ins[b->n - 1].imm2 = ssa_mb[b->ins[b->n - 1].imm2];
            b->succ[b->nsucc++] = b->ins[b->n - 1].imm2;
        }
        if (next >= 0)
            b->succ[b->nsucc++] = next;
    }
}

/* The phis' copies, each edge's in one parallel copy: at the end of a
 * block with one way out, and in a block of the edge's own where there are
 * two -- a branch's taken edge retargeted there, its other placed to be
 * fallen into. */
static int *edge_after;         /* by machine block: the edge block after it */

static int new_pcopy(void)
{
    GROW(pc, npc, pc_cap);
    pc[npc].dst = pc[npc].src = NULL;
    pc[npc].n = 0;

    return npc++;
}

static void pcopy_add(int which, int dst, int src)
{
    PCopy *p = &pc[which];

    p->dst = realloc(p->dst, ((size_t) p->n + 1) * sizeof *p->dst);
    p->src = realloc(p->src, ((size_t) p->n + 1) * sizeof *p->src);
    if (!p->dst || !p->src)
        acc_error("out of memory for the machine IR");
    p->dst[p->n] = dst;
    p->src[p->n] = src;
    p->n++;
}

static int mi_uses(const MIns *mi, int *out);
static int mi_defs(const MIns *mi, int *out);

/* By register: how many instructions read it, before the copies; and the
 * one instruction that writes it, block and place, or def_blk -1 for none
 * or more than one. */
static int *vr_reads, *def_blk, *def_at;

/* A phi's copy from a step of the phi itself -- p++ round a loop: the step
 * made in the phi's register instead, last in the block the copy is made
 * in, and the copy of it a copy of the register into itself, which comes
 * to nothing. The phi's register lives through the whole loop, so that the
 * step's own could not share it. Where the step's value is read by nothing
 * else -- the copy on the way, and the phi, the one phi on this edge.
 * Moved to the end of the block, it adds the same: the phi's register is
 * written nowhere else, but by the copies into the phi's block, and the
 * step comes after the last of those on every path from it to here, since
 * what it makes reaches here. `once` says, by copy, that the value copied
 * goes to that phi alone. */
static void step_in_place(MBlock *f, int which, const unsigned char *once)
{
    PCopy *p = &pc[which];
    int k, j;

    for (k = 0; k != p->n; k++) {
        int dst = p->dst[k], src = p->src[k], t = src;
        MIns *step, *mi;

        if (!once[k] || vr_reads[src] != 0 || def_blk[src] < 0)
            continue;
        for (j = 0; j != p->n; j++)
            if (j != k && (p->src[j] == dst || p->src[j] == src))
                break;
        if (j != p->n)
            continue;
        mi = &mb[def_blk[src]].ins[def_at[src]];
        if (mi->op == M_COPY) {
            t = mi->a;
            if (vr_reads[t] != 1 || def_blk[t] < 0)
                continue;
        }
        step = &mb[def_blk[t]].ins[def_at[t]];
        if (step->op != M_STEP24 || step->a != dst)
            continue;
        cur = (int) (f - mb);
        mi = mi3(M_STEP24, dst, dst, -1);
        step = &mb[def_blk[t]].ins[def_at[t]];         /* mi3 may move it */
        mi->imm = step->imm;
        mi->t = step->t;
        /* What it made, and the copy of that: the phi before the step, read
         * by nothing now, and taken out with the dead code. */
        step->op = M_COPY;
        p->src[k] = dst;
    }
}

static int place_phis(void)
{
    int blk, pred, phi, i, nedges = 0, *phi_head, *phi_next;

    for (blk = 0; blk != nblocks; blk++)
        nedges += preds[blk].count;
    edge_after = realloc(edge_after, ((size_t) nblocks + 1) * sizeof *edge_after);
    taken_head = realloc(taken_head, ((size_t) nblocks + 1) * sizeof *taken_head);
    taken_next = realloc(taken_next, ((size_t) nblocks + nedges + 1) * sizeof *taken_next);
    phi_head = malloc(((size_t) nblocks + 1) * sizeof *phi_head);
    phi_next = malloc(((size_t) nphis + 1) * sizeof *phi_next);
    if (!edge_after || !taken_head || !taken_next || !phi_head || !phi_next)
        acc_error("out of memory for the machine IR");
    for (i = 0; i != nblocks; i++)
        edge_after[i] = taken_head[i] = phi_head[i] = -1;
    vr_reads = realloc(vr_reads, ((size_t) nvr + 1) * sizeof *vr_reads);
    def_blk = realloc(def_blk, ((size_t) nvr + 1) * sizeof *def_blk);
    def_at = realloc(def_at, ((size_t) nvr + 1) * sizeof *def_at);
    if (!vr_reads || !def_blk || !def_at)
        acc_error("out of memory for the machine IR");
    for (i = 0; i != nvr; i++) {
        vr_reads[i] = 0;
        def_blk[i] = def_at[i] = -1;
    }
    for (blk = 0; blk != nmb; blk++)
        for (i = 0; i != mb[blk].n; i++) {
            const MIns *mi = &mb[blk].ins[i];
            int ops[8], n = mi_uses(mi, ops);

            while (n--)
                vr_reads[ops[n]]++;
            n = mi_defs(mi, ops);
            while (n--) {
                def_blk[ops[n]] = def_blk[ops[n]] == -1 ? blk : -2;
                def_at[ops[n]] = i;
            }
        }

    /* The phis, a list for each block: each looked at once an edge. */
    for (phi = nphis - 1; phi >= 0; phi--)
        if (phis[phi].live) {
            phi_next[phi] = phi_head[phis[phi].block];
            phi_head[phis[phi].block] = phi;
        }
    for (blk = 0; blk != nblocks; blk++) {
        if (rpo_num[blk] < 0 && blk)
            continue;
        for (pred = 0; pred != preds[blk].count; pred++) {
            int from = preds[blk].at[pred], which = -1;
            unsigned char once[MAX_PCOPY];
            MBlock *f;

            if (rpo_num[from] < 0 && from)
                continue;
            for (phi = phi_head[blk]; phi >= 0; phi = phi_next[phi]) {
                const Phi *join = &phis[phi];
                int src, dst;

                if (val_vr[join->val] < 0)
                    continue;
                src = join->in[pred];
                if (src < 0)
                    continue;
                dst = val_vr[join->val];
                if (val_vr[src] < 0 || vr[val_vr[src]].width != vr[dst].width) {
                    mir_why = "a phi from a value not in a register of its width";
                    goto out;
                }
                if (which < 0)
                    which = new_pcopy();
                if (pc[which].n == MAX_PCOPY) {
                    mir_why = "a join with more phis than a copy takes";
                    goto out;
                }
                once[pc[which].n] = use_n[src] == 2;   /* a phi's is two */
                pcopy_add(which, dst, val_vr[src]);
            }
            if (which < 0)
                continue;
            f = &mb[ssa_mb[from]];
            if (f->nsucc == 1) {
                /* Before the jump, or at the end where it falls. */
                MIns *mi, last;
                int jumps = f->n && f->ins[f->n - 1].op == M_JMP;

                if (jumps)
                    last = f->ins[--f->n];
                step_in_place(f, which, once);
                cur = ssa_mb[from];
                mi = mi3(M_PCOPY, -1, -1, -1);
                mi->imm = which;
                if (jumps) {
                    GROW(f->ins, f->n, f->cap);
                    f->ins[f->n++] = last;
                }
            } else {
                /* A block of the edge's own: the branch's target, made
                 * after it -- or the block it falls into. */
                int e = new_mb(-1), k;
                MIns *mi;

                f = &mb[ssa_mb[from]];
                step_in_place(&mb[e], which, once);
                cur = e;
                mi = mi3(M_PCOPY, -1, -1, -1);
                mi->imm = which;
                mi = mi3(M_JMP, -1, -1, -1);
                mi->imm2 = ssa_mb[blk];
                mb[e].succ[0] = ssa_mb[blk];
                mb[e].nsucc = 1;
                for (k = 0; k != f->nsucc; k++)
                    if (f->succ[k] == ssa_mb[blk]) {
                        f->succ[k] = e;
                        if (k == 0 && f->n && f->ins[f->n - 1].op == M_BR) {
                            f->ins[f->n - 1].imm2 = e;
                            taken_next[e] = taken_head[ssa_mb[from]];
                            taken_head[ssa_mb[from]] = e;
                        } else {
                            edge_after[ssa_mb[from]] = e;
                        }
                        break;
                    }
            }
        }
    }
    free(phi_head);
    free(phi_next);

    return 1;

out:
    free(phi_head);
    free(phi_next);

    return 0;
}

/* ------------------------------------------------------------------ */
/* operands                                                            */

/* What an instruction reads and writes, as virtual registers. A parallel
 * copy reads every source and writes every destination. */
static int mi_uses(const MIns *mi, int *out)
{
    int n = 0, k;

    if (mi->op == M_PCOPY) {
        for (k = 0; k != pc[mi->imm].n; k++)
            out[n++] = pc[mi->imm].src[k];
        return n;
    }
    if (mi->a >= 0)
        out[n++] = mi->a;
    if (mi->b >= 0)
        out[n++] = mi->b;

    return n;
}

static int mi_defs(const MIns *mi, int *out)
{
    int n = 0, k;

    if (mi->op == M_PCOPY) {
        for (k = 0; k != pc[mi->imm].n; k++)
            out[n++] = pc[mi->imm].dst[k];
        return n;
    }
    if (mi->d >= 0)
        out[n++] = mi->d;
    if (mi->t >= 0)
        out[n++] = mi->t;
    if ((mi->kills & 1) && mi->a >= 0)
        out[n++] = mi->a;
    if ((mi->kills & 2) && mi->b >= 0)
        out[n++] = mi->b;

    return n;
}

static int *opbuf, opbuf_cap;

static void opbuf_fit(int n)
{
    if (n > opbuf_cap) {
        opbuf_cap = n + 16;
        opbuf = realloc(opbuf, (size_t) opbuf_cap * sizeof *opbuf);
        if (!opbuf)
            acc_error("out of memory for the machine IR");
    }
}

static int mi_nops(const MIns *mi)
{
    return mi->op == M_PCOPY ? pc[mi->imm].n + 4 : 8;
}

static int popcount(unsigned x)
{
    int n = 0;

    for (; x; x &= x - 1)
        n++;

    return n;
}

/* ------------------------------------------------------------------ */
/* dead code                                                           */

/* What nothing reads, taken out: a copy, a constant, an address, a
 * widening whose register no instruction reads -- left behind by the
 * constants selection made again where they are read, and by the bytes
 * read in place of what widened them. A worklist: each instruction looked
 * at once more for each operand of it that goes. */
static int pure(int op)
{
    switch (op) {
    case M_COPY: case M_LDI: case M_LDSYM: case M_LDA: case M_LDF: case M_LEAF:
    case M_ZEXT: case M_SEXT: case M_TRUNC: case M_ADD24: case M_SUB24:
    case M_STEP24: case M_ALU8: case M_ALU8I: case M_BYTES24:
        return 1;
    }

    return 0;
}

#define M_DEAD NMOPS            /* an instruction taken out, until compacted */

static void dead_code(void)
{
    int *uses = calloc((size_t) nvr + 1, sizeof *uses);
    int *def_head = malloc(((size_t) nvr + 1) * sizeof *def_head);
    int *def_next, *def_blk, *def_at, *work, nwork = 0, ndefs = 0, total = 0;
    int blk, at, k, n;

    for (blk = 0; blk != nmb; blk++)
        total += mb[blk].n;
    def_next = malloc(((size_t) total + 1) * sizeof *def_next);
    def_blk = malloc(((size_t) total + 1) * sizeof *def_blk);
    def_at = malloc(((size_t) total + 1) * sizeof *def_at);
    work = malloc(((size_t) nvr + 1) * sizeof *work);
    if (!uses || !def_head || !def_next || !def_blk || !def_at || !work)
        acc_error("out of memory for the machine IR");
    for (k = 0; k != nvr; k++)
        def_head[k] = -1;
    for (blk = 0; blk != nmb; blk++)
        for (at = 0; at != mb[blk].n; at++) {
            MIns *mi = &mb[blk].ins[at];

            opbuf_fit(mi_nops(mi));
            n = mi_uses(mi, opbuf);
            for (k = 0; k != n; k++)
                uses[opbuf[k]]++;
            if (pure(mi->op) && mi->d >= 0 && !mi->kills) {
                def_blk[ndefs] = blk;
                def_at[ndefs] = at;
                def_next[ndefs] = def_head[mi->d];
                def_head[mi->d] = ndefs++;
            }
        }
    for (k = 0; k != nvr; k++)
        if (!uses[k] && def_head[k] >= 0)
            work[nwork++] = k;
    while (nwork) {
        int v = work[--nwork], e;

        for (e = def_head[v]; e >= 0; e = def_next[e]) {
            MIns *mi = &mb[def_blk[e]].ins[def_at[e]];

            if (mi->op == M_DEAD)
                continue;
            opbuf_fit(mi_nops(mi));
            n = mi_uses(mi, opbuf);
            mi->op = M_DEAD;
            for (k = 0; k != n; k++)
                if (--uses[opbuf[k]] == 0 && def_head[opbuf[k]] >= 0)
                    work[nwork++] = opbuf[k];
        }
        def_head[v] = -1;
    }
    for (blk = 0; blk != nmb; blk++) {
        int put = 0;

        for (at = 0; at != mb[blk].n; at++)
            if (mb[blk].ins[at].op != M_DEAD)
                mb[blk].ins[put++] = mb[blk].ins[at];
        mb[blk].n = put;
    }
    free(uses);
    free(def_head);
    free(def_next);
    free(def_blk);
    free(def_at);
    free(work);
}

/* ------------------------------------------------------------------ */
/* register allocation: linear scan                                    */

/* Each instruction a position, in the order the blocks are made: it reads
 * its operands at 2i and writes at 2i + 1, so that one operand's register
 * may be its answer's. A register's interval runs from its first position
 * to its last -- and on to a loop's end, where it lives into the loop from
 * before it. Then the intervals in order of their starts, each a register
 * none of the intervals still live has, nor one an instruction inside it
 * needs for itself or clobbers. Linear in the code, n log n with the
 * sorting: no register is ever compared with every other. */

static int *layout, nlayout;
static int *blk_pos, *blk_end;          /* by machine block: first, last position */
static int *iv_s, *iv_e;                /* by register: its interval, or -1 */
static int *iv_def;                     /* by register: its first write */
static int *iv_order;

/* Range OR over positions of what each clobbers, for an interval to ask
 * which units something inside it takes: a sparse table, levels of
 * 2^k positions. */
static unsigned *clob;                  /* by position */
static unsigned **clob_tab;
static int npos, clob_levels;

static void clob_build(void)
{
    int k, i;

    clob_levels = 1;
    while ((1 << clob_levels) <= npos)
        clob_levels++;
    clob_tab = realloc(clob_tab, (size_t) clob_levels * sizeof *clob_tab);
    if (!clob_tab)
        acc_error("out of memory for the machine IR");
    for (k = 0; k != clob_levels; k++) {
        clob_tab[k] = realloc(k ? NULL : NULL, ((size_t) npos + 1) * sizeof **clob_tab);
        if (!clob_tab[k])
            acc_error("out of memory for the machine IR");
    }
    for (i = 0; i != npos; i++)
        clob_tab[0][i] = clob[i];
    for (k = 1; k != clob_levels; k++)
        for (i = 0; i + (1 << k) <= npos; i++)
            clob_tab[k][i] = clob_tab[k - 1][i] | clob_tab[k - 1][i + (1 << (k - 1))];
}

static void clob_free(void)
{
    int k;

    for (k = 0; k != clob_levels; k++)
        free(clob_tab[k]);
    clob_levels = 0;
}

static unsigned clob_or(int from, int to)          /* [from, to] */
{
    int k = 0;

    if (from > to)
        return 0;
    while ((1 << (k + 1)) <= to - from + 1)
        k++;

    return clob_tab[k][from] | clob_tab[k][to - (1 << k) + 1];
}

/* Loops in the order the blocks are made: a jump back from a block to one
 * before it. Sorted by where they start, with a sparse table of the
 * furthest end among a run of them. */
static int *loop_s, *loop_e, nloops;
static int **loop_tab, loop_levels;
static int *loop_pmax;          /* by loop: the furthest end among it and those before */

static int by_loop_start(const void *x, const void *y)
{
    int a = *(const int *) x, b = *(const int *) y;

    return loop_s[a] - loop_s[b];
}

static void loops_build(void)
{
    int k, i, *idx, *s2, *e2, cap = 0, b;

    nloops = 0;
    for (k = 0; k != nlayout; k++) {
        MBlock *blk = &mb[layout[k]];

        for (i = 0; i != blk->nsucc; i++)
            if (blk_pos[blk->succ[i]] >= 0 && blk_pos[blk->succ[i]] <= blk_pos[layout[k]]) {
                if (nloops == cap) {
                    cap = cap ? cap * 2 : 16;
                    loop_s = realloc(loop_s, (size_t) cap * sizeof *loop_s);
                    loop_e = realloc(loop_e, (size_t) cap * sizeof *loop_e);
                    if (!loop_s || !loop_e)
                        acc_error("out of memory for the machine IR");
                }
                loop_s[nloops] = blk_pos[blk->succ[i]];
                loop_e[nloops] = blk_end[layout[k]];
                nloops++;
            }
    }
    if (!nloops)
        return;
    idx = malloc((size_t) nloops * sizeof *idx);
    s2 = malloc((size_t) nloops * sizeof *s2);
    e2 = malloc((size_t) nloops * sizeof *e2);
    if (!idx || !s2 || !e2)
        acc_error("out of memory for the machine IR");
    for (k = 0; k != nloops; k++)
        idx[k] = k;
    qsort(idx, (size_t) nloops, sizeof *idx, by_loop_start);
    for (k = 0; k != nloops; k++) {
        s2[k] = loop_s[idx[k]];
        e2[k] = loop_e[idx[k]];
    }
    memcpy(loop_s, s2, (size_t) nloops * sizeof *s2);
    memcpy(loop_e, e2, (size_t) nloops * sizeof *e2);
    free(idx);
    free(s2);
    free(e2);
    loop_levels = 1;
    while ((1 << loop_levels) <= nloops)
        loop_levels++;
    loop_tab = realloc(loop_tab, (size_t) loop_levels * sizeof *loop_tab);
    if (!loop_tab)
        acc_error("out of memory for the machine IR");
    for (b = 0; b != loop_levels; b++) {
        loop_tab[b] = malloc((size_t) nloops * sizeof **loop_tab);
        if (!loop_tab[b])
            acc_error("out of memory for the machine IR");
    }
    memcpy(loop_tab[0], loop_e, (size_t) nloops * sizeof *loop_e);
    loop_pmax = realloc(loop_pmax, (size_t) nloops * sizeof *loop_pmax);
    if (!loop_pmax)
        acc_error("out of memory for the machine IR");
    for (i = 0; i != nloops; i++)
        loop_pmax[i] = i && loop_pmax[i - 1] > loop_e[i] ? loop_pmax[i - 1] : loop_e[i];
    for (b = 1; b != loop_levels; b++)
        for (i = 0; i + (1 << b) <= nloops; i++) {
            int x = loop_tab[b - 1][i], y = loop_tab[b - 1][i + (1 << (b - 1))];

            loop_tab[b][i] = x > y ? x : y;
        }
}

static void loops_free(void)
{
    int b;

    if (!nloops)
        return;
    for (b = 0; b != loop_levels; b++)
        free(loop_tab[b]);
    loop_levels = 0;
}


/* The start of the earliest loop around position `p`, or -1: the first
 * whose prefix of ends reaches `p` is one that does, if it starts at or
 * before `p`. */
static int loop_around_start(int p)
{
    int lo = 0, hi = nloops;

    while (lo < hi) {
        int mid = (lo + hi) / 2;

        if (loop_pmax[mid] >= p)
            hi = mid;
        else
            lo = mid + 1;
    }

    return lo < nloops && loop_s[lo] <= p ? loop_s[lo] : -1;
}

/* The furthest end of the loops around position `p`, or -1: of those
 * starting at or before it, the furthest end, if it reaches `p`. */
static int loop_around_end(int p)
{
    int lo = 0, hi = nloops;

    while (lo < hi) {
        int mid = (lo + hi) / 2;

        if (loop_s[mid] <= p)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo > 0 && loop_pmax[lo - 1] >= p ? loop_pmax[lo - 1] : -1;
}

/* The furthest end of the loops starting in (from, to], or -1. */
static int loop_reach(int from, int to)
{
    int lo = 0, hi = nloops, first, last, k = 0, x, y;

    while (lo < hi) {                   /* the first starting past `from` */
        int mid = (lo + hi) / 2;

        if (loop_s[mid] <= from)
            lo = mid + 1;
        else
            hi = mid;
    }
    first = lo;
    hi = nloops;
    while (lo < hi) {                   /* the first starting past `to` */
        int mid = (lo + hi) / 2;

        if (loop_s[mid] <= to)
            lo = mid + 1;
        else
            hi = mid;
    }
    last = lo - 1;
    if (first > last)
        return -1;
    while ((1 << (k + 1)) <= last - first + 1)
        k++;
    x = loop_tab[k][first];
    y = loop_tab[k][last - (1 << k) + 1];

    return x > y ? x : y;
}

/* Copies, as each register's partners: the register the other side has,
 * or must have, is the one to try first. */
static int *part_head, *part_next, *part_of, nparts, parts_cap;

static void partner(int x, int y)
{
    int k;

    for (k = 0; k != 2; k++) {
        int a = k ? y : x, b = k ? x : y;

        if (nparts == parts_cap) {
            parts_cap = parts_cap ? parts_cap * 2 : 256;
            part_next = realloc(part_next, (size_t) parts_cap * sizeof *part_next);
            part_of = realloc(part_of, (size_t) parts_cap * sizeof *part_of);
            if (!part_next || !part_of)
                acc_error("out of memory for the machine IR");
        }
        part_of[nparts] = b;
        part_next[nparts] = part_head[a];
        part_head[a] = nparts++;
    }
}

/* By register: the one it is a copy of, where it is written once, by a
 * copy from one that is written once too -- the two hold one value as
 * long as both live, and may share a register, the copy then nothing. A
 * pointer copied into IY for each member read is IY itself, so. -1 for
 * none. */
static int *copy_src, *ndefs;

static int value_of(int v)
{
    return copy_src[v] >= 0 ? copy_src[v] : v;
}

static int same_value(int x, int y)
{
    return value_of(x) == value_of(y);
}

/* Each register's interval stretched over every block it is live in: from
 * each use, back through the blocks before it to its definitions -- a
 * value made in a loop and read after it, carried round the jump back,
 * lives through the whole of the loop, which the order the blocks are
 * made in does not show. Each register's walk is the blocks it is live
 * in, each looked at once, through marks stamped with the register. */
static int *livein_blk, *livein_v, nlivein, livein_cap;

static void livein_add(int blk, int v)
{
    if (nlivein == livein_cap) {
        livein_cap = livein_cap ? 2 * livein_cap : 256;
        livein_blk = realloc(livein_blk, (size_t) livein_cap * sizeof *livein_blk);
        livein_v = realloc(livein_v, (size_t) livein_cap * sizeof *livein_v);
        if (!livein_blk || !livein_v)
            acc_error("out of memory for the machine IR");
    }
    livein_blk[nlivein] = blk;
    livein_v[nlivein++] = v;
}

static void live_through(void)
{
    int *pred_off = calloc((size_t) nmb + 2, sizeof *pred_off), *pred_at, pass;
    int *use_off = calloc((size_t) nvr + 2, sizeof *use_off), *use_blk = NULL, *use_pos = NULL;
    int *def_off = calloc((size_t) nvr + 2, sizeof *def_off), *def_blk = NULL, *def_pos = NULL;
    int *seen = malloc(((size_t) nmb + 1) * sizeof *seen);
    int *def_stamp = malloc(((size_t) nmb + 1) * sizeof *def_stamp);
    int *def_first = malloc(((size_t) nmb + 1) * sizeof *def_first);
    int *work = malloc(((size_t) nmb + 1) * sizeof *work);
    int k, at, n, v, b, nuses = 0, ndef = 0, pos;

    if (!pred_off || !use_off || !def_off || !seen || !def_stamp || !def_first || !work)
        acc_error("out of memory for the machine IR");

    /* Predecessors of the blocks laid out, and the uses and definitions
     * of each register, by block and position, as lists. */
    for (k = 0; k != nlayout; k++)
        for (n = 0; n != mb[layout[k]].nsucc; n++)
            pred_off[mb[layout[k]].succ[n] + 2]++;
    for (b = 0; b != nmb; b++)
        pred_off[b + 2] += pred_off[b + 1];
    pred_at = malloc(((size_t) pred_off[nmb + 1] + 1) * sizeof *pred_at);
    if (!pred_at)
        acc_error("out of memory for the machine IR");
    for (k = 0; k != nlayout; k++)
        for (n = 0; n != mb[layout[k]].nsucc; n++)
            pred_at[pred_off[mb[layout[k]].succ[n] + 1]++] = layout[k];
    for (pass = 0; pass != 2; pass++) {
        pos = 0;
        for (k = 0; k != nlayout; k++) {
            MBlock *blk = &mb[layout[k]];

            for (at = 0; at != blk->n; at++, pos += 2) {
                int m;

                opbuf_fit(mi_nops(&blk->ins[at]));
                m = mi_uses(&blk->ins[at], opbuf);
                while (m--) {
                    v = opbuf[m];
                    if (pass == 0) {
                        use_off[v + 2]++;
                        nuses++;
                    } else {
                        use_blk[use_off[v + 1]] = layout[k];
                        use_pos[use_off[v + 1]++] = pos;
                    }
                }
                m = mi_defs(&blk->ins[at], opbuf);
                while (m--) {
                    v = opbuf[m];
                    if (pass == 0) {
                        def_off[v + 2]++;
                        ndef++;
                    } else {
                        def_blk[def_off[v + 1]] = layout[k];
                        def_pos[def_off[v + 1]++] = pos;
                    }
                }
            }
        }
        if (pass == 0) {
            for (v = 0; v != nvr; v++) {
                use_off[v + 2] += use_off[v + 1];
                def_off[v + 2] += def_off[v + 1];
            }
            use_blk = malloc(((size_t) nuses + 1) * sizeof *use_blk);
            use_pos = malloc(((size_t) nuses + 1) * sizeof *use_pos);
            def_blk = malloc(((size_t) ndef + 1) * sizeof *def_blk);
            def_pos = malloc(((size_t) ndef + 1) * sizeof *def_pos);
            if (!use_blk || !use_pos || !def_blk || !def_pos)
                acc_error("out of memory for the machine IR");
        }
    }

    for (b = 0; b != nmb; b++)
        seen[b] = def_stamp[b] = -1;
    nlivein = 0;
    for (v = 0; v != nvr; v++) {
        int nwork = 0, u;

        for (k = def_off[v]; k != def_off[v + 1]; k++)
            if (def_stamp[def_blk[k]] != v || def_pos[k] < def_first[def_blk[k]]) {
                def_stamp[def_blk[k]] = v;
                def_first[def_blk[k]] = def_pos[k];
            }
        for (u = use_off[v]; u != use_off[v + 1]; u++) {
            b = use_blk[u];
            if (def_stamp[b] == v && def_first[b] < use_pos[u])
                continue;                       /* made before, in the block */
            if (blk_pos[b] < iv_s[v])
                iv_s[v] = blk_pos[b];
            if (seen[b] != v) {
                seen[b] = v;
                work[nwork++] = b;
                livein_add(b, v);
            }
        }
        while (nwork) {
            b = work[--nwork];
            for (k = pred_off[b]; k != pred_off[b + 1]; k++) {
                int p = pred_at[k];

                if (blk_pos[p] < 0)
                    continue;
                if (blk_end[p] + 1 > iv_e[v])
                    iv_e[v] = blk_end[p] + 1;   /* live out of it */
                if (def_stamp[p] == v || seen[p] == v)
                    continue;
                seen[p] = v;
                if (blk_pos[p] < iv_s[v])
                    iv_s[v] = blk_pos[p];
                work[nwork++] = p;
                livein_add(p, v);
            }
        }
    }
    free(pred_off);
    free(pred_at);
    free(use_off);
    free(use_blk);
    free(use_pos);
    free(def_off);
    free(def_blk);
    free(def_pos);
    free(seen);
    free(def_stamp);
    free(def_first);
    free(work);
}

/* The intervals and what goes with them, from the code as it is now. */
static void intervals(void)
{
    int k, at, pos = 0, n, v, total = 0;

    for (k = 0; k != nlayout; k++)
        total += mb[layout[k]].n;
    npos = 2 * total + 2;
    iv_s = realloc(iv_s, ((size_t) nvr + 1) * sizeof *iv_s);
    iv_e = realloc(iv_e, ((size_t) nvr + 1) * sizeof *iv_e);
    iv_def = realloc(iv_def, ((size_t) nvr + 1) * sizeof *iv_def);
    blk_pos = realloc(blk_pos, ((size_t) nmb + 1) * sizeof *blk_pos);
    blk_end = realloc(blk_end, ((size_t) nmb + 1) * sizeof *blk_end);
    clob = realloc(clob, ((size_t) npos + 1) * sizeof *clob);
    part_head = realloc(part_head, ((size_t) nvr + 1) * sizeof *part_head);
    copy_src = realloc(copy_src, ((size_t) nvr + 1) * sizeof *copy_src);
    ndefs = realloc(ndefs, ((size_t) nvr + 1) * sizeof *ndefs);
    if (!iv_s || !iv_e || !iv_def || !blk_pos || !blk_end || !clob || !part_head
        || !copy_src || !ndefs)
        acc_error("out of memory for the machine IR");
    memset(clob, 0, ((size_t) npos + 1) * sizeof *clob);
    nparts = 0;
    for (v = 0; v != nvr; v++) {
        iv_s[v] = iv_e[v] = iv_def[v] = -1;
        vr[v].weight = 0;
        part_head[v] = -1;
        copy_src[v] = -1;
        ndefs[v] = 0;
    }
    for (k = 0; k != nmb; k++)
        blk_pos[k] = blk_end[k] = -1;
    for (k = 0; k != nlayout; k++) {
        MBlock *b = &mb[layout[k]];
        int depth = b->ssa_block >= 0 ? loop_depth[b->ssa_block] : 0;
        long w = 1L << (3 * (depth > 5 ? 5 : depth));

        blk_pos[layout[k]] = pos;
        for (at = 0; at != b->n; at++, pos += 2) {
            MIns *mi = &b->ins[at];

            opbuf_fit(mi_nops(mi));
            n = mi_uses(mi, opbuf);
            while (n--) {
                v = opbuf[n];
                if (iv_s[v] < 0 || pos < iv_s[v])
                    iv_s[v] = pos;
                if (pos > iv_e[v])
                    iv_e[v] = pos;
                vr[v].weight += w;
            }
            n = mi_defs(mi, opbuf);
            while (n--) {
                v = opbuf[n];
                /* A clobbered register is read too, at the same time. */
                int at_pos = v == mi->t ? pos : pos + 1;

                if (iv_s[v] < 0 || at_pos < iv_s[v])
                    iv_s[v] = at_pos;
                if (pos + 1 > iv_e[v])
                    iv_e[v] = pos + 1;
                if (iv_def[v] < 0)
                    iv_def[v] = at_pos;
                vr[v].weight += w;
                ndefs[v]++;
            }
            if (mi->op == M_HELPER)
                clob[pos] |= UB(U_A);
            if (mi->op == M_CALL)
                clob[pos] |= UB(U_A) | preg_units[P_HL];
            if (mi->op == M_COPY || mi->op == M_STEP24 || mi->op == M_TRUNC
                || mi->op == M_BYTES24)
                partner(mi->d, mi->a);
            if (mi->op == M_PCOPY)
                for (n = 0; n != pc[mi->imm].n; n++)
                    partner(pc[mi->imm].dst[n], pc[mi->imm].src[n]);
        }
        blk_end[layout[k]] = pos > 0 ? pos - 1 : 0;
        if (blk_end[layout[k]] < blk_pos[layout[k]])
            blk_end[layout[k]] = blk_pos[layout[k]];
    }
    npos = pos + 2;
    live_through();

    /* The copies that make one value: each side written once, of a width,
     * and the source no copy itself -- so that copies of one share it. */
    for (k = 0; k != nlayout; k++) {
        MBlock *b = &mb[layout[k]];

        for (at = 0; at != b->n; at++) {
            const MIns *mi = &b->ins[at];

            if (mi->op == M_COPY && ndefs[mi->d] == 1 && ndefs[mi->a] == 1
                && vr[mi->d].width == vr[mi->a].width && iv_def[mi->a] >= 0)
                copy_src[mi->d] = mi->a;
        }
    }
    for (v = 0; v != nvr; v++)
        if (copy_src[v] >= 0 && copy_src[copy_src[v]] >= 0)
            copy_src[v] = -1;

    /* Living into a loop from before it: on to its end, and to the end of
     * any loop that reaches into. */
    loops_build();
    if (nloops)
        for (v = 0; v != nvr; v++) {
            int reach, guard = 0;

            if (iv_s[v] < 0)
                continue;
            /* Read before it is first written: it comes round a loop's
             * jump back, so it lives through every loop around its ends.
             * A parameter's is written where the function begins. */
            if (iv_def[v] < 0 || iv_s[v] < iv_def[v])
                while (guard++ < 64) {
                    int s2 = loop_around_start(iv_s[v]), e2 = loop_around_end(iv_e[v]);

                    if ((s2 < 0 || s2 >= iv_s[v]) && (e2 < 0 || e2 <= iv_e[v]))
                        break;
                    if (s2 >= 0 && s2 < iv_s[v])
                        iv_s[v] = s2;
                    if (e2 > iv_e[v])
                        iv_e[v] = e2;
                }
            while ((reach = loop_reach(iv_s[v], iv_e[v])) > iv_e[v] && guard++ < 128)
                iv_e[v] = reach;
        }
    loops_free();
    clob_build();
}

/* Intervals of registers that must be one register -- an operand's copy
 * into HL, A, a clobbered BC -- by unit, in order: what a register may not
 * be given across. */
static int **fix_s, **fix_e, **fix_v, fix_n[U_F + 1], fix_cap[U_F + 1];

static void fixed_build(int n)
{
    int u, k;

    for (u = 0; u <= U_F; u++)
        fix_n[u] = 0;
    if (!fix_s) {
        fix_s = calloc(U_F + 1, sizeof *fix_s);
        fix_e = calloc(U_F + 1, sizeof *fix_e);
        fix_v = calloc(U_F + 1, sizeof *fix_v);
        if (!fix_s || !fix_e || !fix_v)
            acc_error("out of memory for the machine IR");
    }
    for (k = 0; k != n; k++) {             /* in order of start */
        int v = iv_order[k], p;
        unsigned units;

        if (iv_s[v] < 0 || popcount(vr[v].cls) != 1)
            continue;
        for (p = 0; !(vr[v].cls & PB(p)); p++)
            ;
        units = preg_units[p];
        for (u = 0; u <= U_F; u++)
            if (units & UB(u)) {
                if (fix_n[u] == fix_cap[u]) {
                    fix_cap[u] = fix_cap[u] ? fix_cap[u] * 2 : 64;
                    fix_s[u] = realloc(fix_s[u], (size_t) fix_cap[u] * sizeof **fix_s);
                    fix_e[u] = realloc(fix_e[u], (size_t) fix_cap[u] * sizeof **fix_e);
                    fix_v[u] = realloc(fix_v[u], (size_t) fix_cap[u] * sizeof **fix_v);
                    if (!fix_s[u] || !fix_e[u] || !fix_v[u])
                        acc_error("out of memory for the machine IR");
                }
                fix_s[u][fix_n[u]] = iv_s[v];
                fix_e[u][fix_n[u]] = iv_e[v];
                fix_v[u][fix_n[u]] = v;
                fix_n[u]++;
            }
    }
}

/* Whether a fixed interval other than `v`'s own, on a unit of `units`,
 * overlaps [s, e]: the fixed ones on a unit sorted by start, and from the
 * last starting at or before `e`, back over those whose ends reach `s`. */
static int fixed_clash(unsigned units, int s, int e, int v)
{
    int u;

    for (u = 0; u <= U_F; u++) {
        int lo = 0, hi = fix_n[u], k;

        if (!(units & UB(u)))
            continue;
        while (lo < hi) {
            int mid = (lo + hi) / 2;

            if (fix_s[u][mid] <= e)
                lo = mid + 1;
            else
                hi = mid;
        }
        /* Those on one unit do not overlap one another, so only the last
         * two can reach back -- the one before `v` being `v` itself. */
        for (k = lo - 1; k >= 0 && k >= lo - 2; k--)
            if (fix_e[u][k] >= s && fix_v[u][k] != v && !same_value(fix_v[u][k], v))
                return 1;
    }

    return 0;
}

static int by_start(const void *x, const void *y)
{
    int a = *(const int *) x, b = *(const int *) y;

    if (iv_s[a] != iv_s[b])
        return iv_s[a] - iv_s[b];
    if (popcount(vr[a].cls) != popcount(vr[b].cls))
        return popcount(vr[a].cls) - popcount(vr[b].cls);

    return a - b;
}

/* The register a partner of `v` has, or must have, that suits `v`: a
 * byte's of a pair's low byte, a pair's of the pair. */
/* A copy's partner as the scan has it now: with intervals split, the part
 * of its value that holds a register now, where one does; and a part's
 * partners are its value's. Without splitting, each is itself. */
static int split_on;
static int *fam;                /* by register: the one it was cut from first */
static int *act, nact, act_cap; /* the parts in a register now */
static int va_cap;              /* the room the by-register arrays have */

static int part_root(int v)
{
    return split_on && v < va_cap ? fam[v] : v;
}

static int part_now(int o)
{
    int j, r = part_root(o);

    if (!split_on)
        return o;
    for (j = 0; j != nact; j++)
        if (part_root(act[j]) == r)
            return act[j];

    return o;
}

static int partner_reg(int v, int p_ok(int, int), int fixed_too)
{
    int e, from = v, round;

    for (round = 0; round != 2; round++, from = part_root(v)) {
        if (round && from == v)
            break;
    for (e = part_head[from]; e >= 0; e = part_next[e]) {
        int o = part_now(part_of[e]), p = vr[o].preg;

        if (p < 0 && fixed_too && popcount(vr[o].cls) == 1)
            for (p = 0; !(vr[o].cls & PB(p)); p++)
                ;
        if (p < 0)
            continue;
        if (vr[v].width == 1 && vr[o].width == 3)
            p = low_of(p);
        else if (vr[v].width != vr[o].width)
            continue;
        if (p >= 0 && p_ok(v, p))
            return p;
    }
    }

    return -1;
}

/* Copy groups: registers joined by copies, of one width, through a
 * union-find -- and for each, the register its members that must be one
 * register want most, which every member tries first. What graph
 * colouring's coalescing did by merging, as a preference, in near-linear
 * time. */
static int *grp, *grp_pref;

static int grp_find(int v)
{
    while (grp[v] != v)
        v = grp[v] = grp[grp[v]];

    return v;
}

static void groups_build(void)
{
    int v, k, *count;

    grp = realloc(grp, ((size_t) nvr + 1) * sizeof *grp);
    grp_pref = realloc(grp_pref, ((size_t) nvr + 1) * sizeof *grp_pref);
    count = calloc(((size_t) nvr + 1) * NPREGS, sizeof *count);
    if (!grp || !grp_pref || !count)
        acc_error("out of memory for the machine IR");
    for (v = 0; v != nvr; v++)
        grp[v] = v;
    for (v = 0; v != nvr; v++)
        for (k = part_head[v]; k >= 0; k = part_next[k]) {
            int o = part_of[k], x, y;

            if (vr[o].width != vr[v].width)
                continue;
            x = grp_find(v);
            y = grp_find(o);
            if (x != y)
                grp[x] = y;
        }
    for (v = 0; v != nvr; v++) {
        int p;

        grp_pref[v] = -1;
        if (iv_s[v] < 0 || popcount(vr[v].cls) != 1)
            continue;
        for (p = 0; !(vr[v].cls & PB(p)); p++)
            ;
        count[(size_t) grp_find(v) * NPREGS + p] += 1;
    }
    for (v = 0; v != nvr; v++) {
        int r = grp_find(v), p, best = -1;

        if (r != v)
            continue;
        for (p = 0; p != NPREGS; p++)
            if (count[(size_t) r * NPREGS + p] && (best < 0
                || count[(size_t) r * NPREGS + p] > count[(size_t) r * NPREGS + best]))
                best = p;
        grp_pref[r] = best;
    }
    free(count);
}

static unsigned scan_busy;              /* the units of the intervals live */

static int reg_ok(int v, int p)
{
    return (vr[v].cls & PB(p)) && !(preg_units[p] & scan_busy)
           && !(preg_units[p] & clob_or(iv_s[v], iv_e[v] - 1))
           && !fixed_clash(preg_units[p], iv_s[v], iv_e[v], v);
}

static int *spilled;            /* by register: to be spilled, this round */

/* What spilling a register costs, for the scan to spill the cheapest:
 * its weighted uses -- halved where it is made again or read from its
 * own slot rather than stored -- for each position it holds a register,
 * so that a long life used seldom goes first. */
static long spill_cost(int v)
{
    long length = iv_e[v] - iv_s[v] + 1;

    return vr[v].weight * (vr[v].remat || vr[v].param ? 1 : 2) * 4096 / length;
}

/* One scan: each interval a register, or marked to be spilled. Answers
 * how many were, or -1 where one that no spill helps could have none. */
static int linear_scan(void)
{
    int k, n = 0, nactive = 0, active[NPREGS + 8];

    iv_order = realloc(iv_order, ((size_t) nvr + 1) * sizeof *iv_order);
    spilled = realloc(spilled, ((size_t) nvr + 1) * sizeof *spilled);
    if (!iv_order || !spilled)
        acc_error("out of memory for the machine IR");
    for (k = 0; k != nvr; k++) {
        iv_order[n] = k;
        n += iv_s[k] >= 0;
        vr[k].preg = -1;
        spilled[k] = 0;
    }
    qsort(iv_order, (size_t) n, sizeof *iv_order, by_start);
    fixed_build(n);
    groups_build();
    for (k = 0; k != n; k++) {
        int v = iv_order[k], j, p, nsp = 0;

        for (j = 0; j < nactive; j++)                   /* expired */
            if (iv_e[active[j]] < iv_s[v])
                active[j--] = active[--nactive];
        for (;;) {
            scan_busy = 0;
            for (j = 0; j != nactive; j++)
                if (!same_value(active[j], v))
                    scan_busy |= preg_units[vr[active[j]].preg];
            /* A copy partner's register, where it has one; the group's;
             * a partner's that must be one register; any. */
            p = partner_reg(v, reg_ok, 0);
            if (p < 0) {
                p = grp_pref[grp_find(v)];
                if (p >= 0 && !reg_ok(v, p))
                    p = -1;
            }
            if (p < 0)
                p = partner_reg(v, reg_ok, 1);
            if (p < 0)
                for (p = 0; p != NPREGS && !reg_ok(v, p); p++)
                    ;
            if (p < NPREGS)
                break;
            /* None: the cheapest of the live ones in a register `v` could
             * have, or `v` itself, to be spilled. */
            {
                int victim = -1, jv = -1;
                long cost, best = 0;

                for (j = 0; j != nactive; j++) {
                    int a = active[j];

                    if (vr[a].short_lived || !(vr[v].cls & PB(vr[a].preg)))
                        continue;
                    cost = spill_cost(a);
                    if (victim < 0 || cost < best) {
                        victim = a;
                        jv = j;
                        best = cost;
                    }
                }
                cost = spill_cost(v);
                if (!vr[v].short_lived && (victim < 0 || cost <= best)) {
                    spilled[v] = 1;
                    p = -1;
                    break;
                }
                if (victim < 0 || ++nsp > NPREGS)
                    return -1;
                spilled[victim] = 1;
                vr[victim].preg = -1;
                active[jv] = active[--nactive];
            }
        }
        if (p < 0)
            continue;
        vr[v].preg = p;
        active[nactive++] = v;
    }
    n = 0;
    for (k = 0; k != nvr; k++)
        n += spilled[k];
    clob_free();

    return n;
}

/* ------------------------------------------------------------------ */
/* spilling                                                            */

static int *spill_size, nspills, spills_cap;

static int new_spill(int width)
{
    GROW(spill_size, nspills, spills_cap);
    spill_size[nspills] = width;

    return ++nspills;                   /* 1-based: 0 is none */
}

/* Every register marked kept in a frame slot -- or made again where it is
 * used, a constant or an address, or read from its parameter's slot -- and
 * a register of its own for each read and write, which is a short one: in
 * one pass over the code, each block made again. */
static MIns *sp_out;
static int sp_n, sp_cap;

static void sp_put(const MIns *mi)
{
    GROW(sp_out, sp_n, sp_cap);
    sp_out[sp_n++] = *mi;
}

/* A short register loaded with spilled `v`, the load put out first. */
static int reload(int v)
{
    int t = new_vr(vr[v].width, vr[v].cls);
    MIns load;

    vr[t].short_lived = 1;
    memset(&load, 0, sizeof load);
    load.d = t;
    load.a = load.b = load.t = load.sym = -1;
    load.width = vr[v].width;
    if (vr[v].remat) {
        load.op = vr[v].remat;
        load.imm = vr[v].remat_imm;
        load.sym = vr[v].remat == M_LDSYM ? vr[v].remat_sym : -1;
        load.imm2 = vr[v].remat == M_LDA ? vr[v].remat_sym : 0;
        vr[t].remat = vr[v].remat;
        vr[t].remat_imm = vr[v].remat_imm;
        vr[t].remat_sym = vr[v].remat_sym;
    } else {
        load.op = M_LDF;
        load.imm = vr[v].param ? vr[v].param : vr[v].spill;
        load.sym = vr[v].param ? -1 : -2;               /* -2: a spill's */
    }
    sp_put(&load);

    return t;
}

/* A short register for a write of spilled `v`, and its store after. */
static int restore(int v, MIns *after, int *nafter)
{
    int t = new_vr(vr[v].width, vr[v].cls);

    vr[t].short_lived = 1;
    if (vr[v].spill) {
        MIns *store = &after[(*nafter)++];

        memset(store, 0, sizeof *store);
        store->op = M_STF;
        store->a = t;
        store->d = store->b = store->t = -1;
        store->sym = -2;
        store->imm = vr[v].spill;
        store->width = vr[v].width;
    }

    return t;
}

static void spill_all(void)
{
    int blk, v;

    for (v = 0; v != nvr; v++)
        if (spilled[v] && !vr[v].remat && !vr[v].param && !vr[v].spill)
            vr[v].spill = new_spill(vr[v].width);
    for (blk = 0; blk != nmb; blk++) {
        MBlock *b = &mb[blk];
        int at, k;

        sp_n = 0;
        for (at = 0; at != b->n; at++) {
            MIns mi = b->ins[at], after[MAX_PCOPY + 2];
            int nafter = 0;

            /* The definition of a constant, an address or a parameter is
             * dropped: made again where read. */
            if (mi.d >= 0 && spilled[mi.d] && (vr[mi.d].remat || vr[mi.d].param)
                && (mi.op == M_LDI || mi.op == M_LDSYM || mi.op == M_LDF))
                continue;
            if (mi.op == M_PCOPY) {
                PCopy *p = &pc[mi.imm];

                for (k = 0; k != p->n; k++)
                    if (spilled[p->src[k]])
                        p->src[k] = reload(p->src[k]);
                for (k = 0; k != p->n; k++)
                    if (spilled[p->dst[k]])
                        p->dst[k] = restore(p->dst[k], after, &nafter);
            } else {
                if (mi.a >= 0 && spilled[mi.a]) {
                    int t = reload(mi.a);

                    if (mi.b == mi.a)
                        mi.b = t;
                    mi.a = t;
                }
                if (mi.b >= 0 && spilled[mi.b])
                    mi.b = reload(mi.b);
                if (mi.d >= 0 && spilled[mi.d])
                    mi.d = restore(mi.d, after, &nafter);
            }
            sp_put(&mi);
            for (k = 0; k != nafter; k++)
                sp_put(&after[k]);
        }
        if (sp_n > b->cap) {
            b->ins = realloc(b->ins, (size_t) sp_n * sizeof *b->ins);
            if (!b->ins)
                acc_error("out of memory for the machine IR");
            b->cap = sp_n;
        }
        if (sp_n)
            memcpy(b->ins, sp_out, (size_t) sp_n * sizeof *sp_out);
        b->n = sp_n;
    }
}

/* ------------------------------------------------------------------ */
/* splitting                                                           */

/* The scan with intervals split, as Wimmer and Franz's linear scan does
 * it: where no register is free for the whole of an interval, it has one
 * for as long as one is, and the rest is an interval of its own, back in
 * the queue; where none is free at all, the one live whose next use is
 * furthest gives its register up there and waits in memory until just
 * before that use. Each part is a register of its own -- the uses after
 * the cut renamed -- so that everything after allocation sees registers as
 * it always did. A part in memory has no uses: each use and each write
 * needs a register, and the cut is made before it.
 *
 * Then the moves that join the parts: in a block, where one part ends and
 * the next begins -- stores, then the registers in one parallel copy, then
 * loads; and on each edge, where what a value is in at the end of the
 * block before differs from what it is in at the start of the one after.
 * Each step is a binary search or a heap's: n log n in the code. */
static int *child_next;         /* by register: the next part of its value */
static int *inmem;              /* by register: a part kept in memory */
static int *up_lo, *up_hi;      /* by register: its positions in upos */
static int *upos, nupos;        /* the positions each register is read or
                                 * written at, in order, a run a register */
static int *ins_blk, *ins_idx;  /* by instruction number: where it is */
static int *call_of;            /* by instruction number: the call whose
                                 * arguments it is among -- after the
                                 * pairs are pushed -- as a position, or -1 */
static int splitting;           /* the scan is split_scan */
static int *alloc_src;          /* copy_src as the scan saw it */
static int  alloc_src_n;
static int *heap, nheap, heap_cap;
static int grp_n;               /* registers groups_build saw */
static int ninsn_scan;          /* instructions split_scan numbered */

static int *va_grow(int *arr, int fill)
{
    int k;

    arr = realloc(arr, (size_t) va_cap * sizeof *arr);
    if (!arr)
        acc_error("out of memory for the machine IR");
    for (k = nvr; k != va_cap; k++)
        arr[k] = fill;

    return arr;
}

static void va_fit(void)
{
    if (nvr + 1 <= va_cap)
        return;
    va_cap = 2 * (nvr + 1);
    iv_s = va_grow(iv_s, -1);
    iv_e = va_grow(iv_e, -1);
    iv_def = va_grow(iv_def, -1);
    copy_src = va_grow(copy_src, -1);
    ndefs = va_grow(ndefs, 0);
    part_head = va_grow(part_head, -1);
    fam = va_grow(fam, -1);
    child_next = va_grow(child_next, -1);
    inmem = va_grow(inmem, 0);
    up_lo = va_grow(up_lo, 0);
    up_hi = va_grow(up_hi, 0);
}

static int heap_less(int x, int y)
{
    if (iv_s[x] != iv_s[y])
        return iv_s[x] < iv_s[y];
    if (popcount(vr[x].cls) != popcount(vr[y].cls))
        return popcount(vr[x].cls) < popcount(vr[y].cls);

    return x < y;
}

static void heap_push(int v)
{
    int at;

    GROW(heap, nheap, heap_cap);
    at = nheap++;
    while (at && heap_less(v, heap[(at - 1) / 2])) {
        heap[at] = heap[(at - 1) / 2];
        at = (at - 1) / 2;
    }
    heap[at] = v;
}

static int heap_pop(void)
{
    int top = heap[0], v = heap[--nheap], at = 0;

    for (;;) {
        int kid = 2 * at + 1;

        if (kid >= nheap)
            break;
        if (kid + 1 < nheap && heap_less(heap[kid + 1], heap[kid]))
            kid++;
        if (!heap_less(heap[kid], v))
            break;
        heap[at] = heap[kid];
        at = kid;
    }
    if (nheap)
        heap[at] = v;

    return top;
}

/* The first position at or after `x` that `v` is read or written at, or
 * -1. */
static int next_use(int v, int x)
{
    int lo = up_lo[v], hi = up_hi[v];

    while (lo < hi) {
        int mid = (lo + hi) / 2;

        if (upos[mid] < x)
            lo = mid + 1;
        else
            hi = mid;
    }

    return lo < up_hi[v] ? upos[lo] : -1;
}

/* The first position at or after `s` where something clobbers one of
 * `units`, or -1: a binary search over the range OR. */
static int clob_first(unsigned units, int s)
{
    int lo = s, hi = npos - 1;

    if (s >= npos || !(clob_or(s, npos - 1) & units))
        return -1;
    while (lo < hi) {
        int mid = (lo + hi) / 2;

        if (clob_or(s, mid) & units)
            hi = mid;
        else
            lo = mid + 1;
    }

    return lo;
}

/* The first start, at or after `s`, of an interval fixed to one of `units`
 * other than `v` and its copies -- or `s` itself where one covers it. */
static int fixed_first(unsigned units, int s, int v)
{
    int u, best = -1;

    for (u = 0; u <= U_F; u++) {
        int lo = 0, hi = fix_n[u], k;

        if (!(units & UB(u)))
            continue;
        while (lo < hi) {
            int mid = (lo + hi) / 2;

            if (fix_e[u][mid] < s)
                lo = mid + 1;
            else
                hi = mid;
        }
        for (k = lo; k != fix_n[u]; k++) {
            int f = fix_v[u][k];

            if (f == v || same_value(f, v) || fam[f] == fam[v])
                continue;
            if (best < 0 || (fix_s[u][k] < s ? s : fix_s[u][k]) < best)
                best = fix_s[u][k] < s ? s : fix_s[u][k];
            break;
        }
    }

    return best;
}

/* How far `v` may keep register `p` from its start, whatever holds it
 * now: up to the next interval fixed to it, or the next instruction that
 * clobbers it. -1 where one of those is there already. */
static int limit_until(int v, int p)
{
    unsigned units = preg_units[p];
    int s = iv_s[v], fu = npos, f, q;

    /* Made among a call's arguments, after the pairs live across it are
     * pushed: nothing made there is pushed, so it is in no register at the
     * call -- in memory across it, where it lives across. */
    if ((s >> 1) < ninsn_scan && call_of[s >> 1] >= 0 && call_of[s >> 1] > s)
        fu = call_of[s >> 1] - 1;
    f = fixed_first(units, s, v);
    if (f >= 0) {
        if (f <= s)
            return -1;
        fu = f - 1;
    }
    q = clob_first(units, s);
    if (q >= 0 && q < fu)
        fu = q;

    return fu;
}

/* Whether a limit lets `v` keep a register past its start: for all of it,
 * or up to a cut after its start -- a cut is before an instruction, at an
 * even position. */
static int long_enough(int v, int fu)
{
    return fu >= iv_e[v] || ((fu + 1) & ~1) > iv_s[v];
}

/* The same, and -1 where an interval live now holds `p`. */
static int free_until(int v, int p)
{
    int j;

    for (j = 0; j != nact; j++)
        if ((preg_units[vr[act[j]].preg] & preg_units[p]) && !same_value(act[j], v))
            return -1;

    return limit_until(v, p);
}

/* The instruction at position `pos`. */
static MIns *ins_at(int pos)
{
    return &mb[ins_blk[pos >> 1]].ins[ins_idx[pos >> 1]];
}

/* `v` cut at `sp`, an even position inside it: the part from there on a
 * register of its own, its uses renamed. */
static int split_at(int v, int sp)
{
    int c, lo, hi, k;
    VReg copy = vr[v];

    if (sp <= iv_s[v] || sp > iv_e[v])
        acc_error("internal: an interval cut outside itself");
    c = new_vr(copy.width, copy.cls);
    vr[c] = copy;
    vr[c].preg = -1;
    vr[c].short_lived = 0;
    va_fit();
    inmem[c] = 0;
    copy_src[c] = -1;
    ndefs[c] = 0;
    part_head[c] = -1;
    iv_def[c] = -1;
    iv_s[c] = sp;
    iv_e[c] = iv_e[v];
    iv_e[v] = sp - 1;
    fam[c] = fam[v];
    child_next[c] = child_next[v];
    child_next[v] = c;
    lo = up_lo[v];
    hi = up_hi[v];
    k = lo;
    while (k < hi && upos[k] < sp)
        k++;
    up_lo[c] = k;
    up_hi[c] = hi;
    up_hi[v] = k;
    for (; k != hi; k++) {
        MIns *mi = ins_at(upos[k]);
        int n;

        if (mi->d == v) mi->d = c;
        if (mi->a == v) mi->a = c;
        if (mi->b == v) mi->b = c;
        if (mi->t == v) mi->t = c;
        if (mi->op == M_PCOPY)
            for (n = 0; n != pc[mi->imm].n; n++) {
                if (pc[mi->imm].src[n] == v) pc[mi->imm].src[n] = c;
                if (pc[mi->imm].dst[n] == v) pc[mi->imm].dst[n] = c;
            }
    }

    return c;
}

/* Where a part in memory until its next use `nu` gives way to the next:
 * before the instruction that reads it, or at the write itself -- where
 * what was in memory is not wanted, and nothing is loaded. */
static int cut_for(int nu)
{
    return nu;
}

/* A part not yet given a place: in memory until its next use, then back
 * in the queue -- or in memory to its end, where it has none. */
static void requeue(int t)
{
    int nu = next_use(t, iv_s[t]), sp;

    if (nu < 0) {
        inmem[t] = 1;
        return;
    }
    sp = cut_for(nu);
    if (sp > iv_s[t]) {
        int rest = split_at(t, sp);

        inmem[t] = 1;
        heap_push(rest);
        return;
    }
    heap_push(t);
}

/* `v` given `p` for as long as it may have it, the rest cut off. */
static void assign(int v, int p)
{
    int fu = free_until(v, p);

    vr[v].preg = p;
    if (fu < iv_e[v])
        requeue(split_at(v, (fu + 1) & ~1));
    GROW(act, nact, act_cap);
    act[nact++] = v;
}

static int cur_fu[NPREGS];

static int fu_whole(int v, int p)
{
    return (vr[v].cls & PB(p)) && cur_fu[p] >= iv_e[v];
}

/* A register free at the start of `v`: one for the whole of it -- a
 * copy's partner's first, then its group's -- or the one free longest,
 * where that is long enough to cut. */
static int assign_free(int v)
{
    int best = -1, p;

    for (p = 0; p != NPREGS; p++)
        cur_fu[p] = vr[v].cls & PB(p) ? free_until(v, p) : -1;

    /* A later part of a value: where its first part is, so that the moves
     * joining them come to nothing where they meet -- round a loop, above
     * all, whose head has the first. */
    p = fam[v] != v ? vr[fam[v]].preg : -1;
    if (p >= 0 && fu_whole(v, p)) {
        assign(v, p);
        return 1;
    }
    p = partner_reg(v, fu_whole, 0);
    if (p < 0 && fam[v] < grp_n) {
        p = grp_pref[grp_find(fam[v])];
        if (p >= 0 && !fu_whole(v, p))
            p = -1;
    }
    if (p < 0)
        p = partner_reg(v, fu_whole, 1);
    if (p < 0)
        for (p = 0; p != NPREGS && !fu_whole(v, p); p++)
            ;
    if (p < NPREGS && p >= 0) {
        assign(v, p);
        return 1;
    }
    for (p = 0; p != NPREGS; p++)
        if (cur_fu[p] >= 0 && (best < 0 || cur_fu[p] > cur_fu[best]))
            best = p;
    if (best >= 0 && long_enough(v, cur_fu[best])) {
        assign(v, best);
        return 1;
    }

    return 0;
}

/* None free at the start of `v`: the register whose holders' next use is
 * furthest, taken from them -- each cut there and kept in memory until
 * that use -- or `v` itself kept in memory until its own, where that is
 * further still and it is not written here. 0 where no register can be
 * had: those holding each are made for one instruction, or read here. */
static int assign_blocked(int v)
{
    int s = iv_s[v], ev = s & ~1, best = -1, best_nu = -1, p, j, cu;

    for (p = 0; p != NPREGS; p++) {
        unsigned units = preg_units[p];
        int nu = INT_MAX, ok = 1, f;

        if (!(vr[v].cls & PB(p)))
            continue;
        f = limit_until(v, p);
        if (f < 0 || !long_enough(v, f))
            continue;
        for (j = 0; j != nact && ok; j++) {
            int a = act[j], na;

            if (!(preg_units[vr[a].preg] & units) || same_value(a, v))
                continue;
            if (vr[a].short_lived || popcount(vr[a].cls) == 1 || ev <= iv_s[a]) {
                ok = 0;
                break;
            }
            na = next_use(a, ev);
            if (na >= 0 && na <= s)
                ok = 0;
            else if (na >= 0 && na < nu)
                nu = na;
        }
        if (ok && nu > best_nu) {
            best = p;
            best_nu = nu;
        }
    }
    if (best < 0)
        return 0;
    cu = next_use(v, s);
    if (cu != s && (cu < 0 || cu > best_nu) && !vr[v].short_lived) {
        if (cu < 0) {
            inmem[v] = 1;
            return 1;
        }
        if (cut_for(cu) > s) {
            heap_push(split_at(v, cut_for(cu)));
            inmem[v] = 1;
            return 1;
        }
    }
    for (j = 0; j < nact; j++) {
        int a = act[j];

        if (!(preg_units[vr[a].preg] & preg_units[best]) || same_value(a, v))
            continue;
        act[j--] = act[--nact];
        requeue(split_at(a, ev));
    }
    assign(v, best);

    return 1;
}

/* The scan. -1 where a register cannot be had for one that must have
 * one. */
static int split_scan(void)
{
    int k, at, pos, n, v, ninsn = 0;

    for (k = 0; k != nlayout; k++)
        ninsn += mb[layout[k]].n;
    ins_blk = realloc(ins_blk, ((size_t) ninsn + 1) * sizeof *ins_blk);
    ins_idx = realloc(ins_idx, ((size_t) ninsn + 1) * sizeof *ins_idx);
    if (!ins_blk || !ins_idx)
        acc_error("out of memory for the machine IR");
    va_cap = nvr + 1;
    fam = realloc(fam, (size_t) va_cap * sizeof *fam);
    child_next = realloc(child_next, (size_t) va_cap * sizeof *child_next);
    inmem = realloc(inmem, (size_t) va_cap * sizeof *inmem);
    up_lo = realloc(up_lo, (size_t) va_cap * sizeof *up_lo);
    up_hi = realloc(up_hi, (size_t) va_cap * sizeof *up_hi);
    if (!fam || !child_next || !inmem || !up_lo || !up_hi)
        acc_error("out of memory for the machine IR");
    for (v = 0; v != nvr; v++) {
        fam[v] = v;
        child_next[v] = -1;
        inmem[v] = 0;
        up_lo[v] = up_hi[v] = 0;
        vr[v].preg = -1;
    }

    /* Each register's positions, in order: a count, then a fill. */
    call_of = realloc(call_of, ((size_t) ninsn + 1) * sizeof *call_of);
    if (!call_of)
        acc_error("out of memory for the machine IR");
    ninsn_scan = ninsn;
    for (pos = 0, k = 0; k != nlayout; k++) {
        int open = -1;

        for (at = 0; at != mb[layout[k]].n; at++, pos += 2) {
            call_of[pos >> 1] = -1;
            if (mb[layout[k]].ins[at].op == M_SAVE) {
                open = pos >> 1;
                continue;
            }
            if (open < 0)
                continue;
            if (mb[layout[k]].ins[at].op == M_CALL) {
                int j;

                for (j = open + 1; j <= (pos >> 1); j++)
                    call_of[j] = pos;
                open = -1;
            }
        }
    }
    for (n = 0, pos = 0, k = 0; k != nlayout; k++)
        for (at = 0; at != mb[layout[k]].n; at++, pos += 2) {
            MIns *mi = &mb[layout[k]].ins[at];
            int m;

            ins_blk[pos >> 1] = layout[k];
            ins_idx[pos >> 1] = at;
            opbuf_fit(mi_nops(mi));
            m = mi_uses(mi, opbuf);
            while (m--)
                up_hi[opbuf[m]]++;
            m = mi_defs(mi, opbuf);
            while (m--)
                up_hi[opbuf[m]]++;
        }
    for (v = 0; v != nvr; v++) {
        up_lo[v] = n;
        n += up_hi[v];
        up_hi[v] = up_lo[v];
    }
    nupos = n;
    upos = realloc(upos, ((size_t) n + 1) * sizeof *upos);
    if (!upos)
        acc_error("out of memory for the machine IR");
    for (pos = 0, k = 0; k != nlayout; k++)
        for (at = 0; at != mb[layout[k]].n; at++, pos += 2) {
            MIns *mi = &mb[layout[k]].ins[at];
            int m;

            m = mi_uses(mi, opbuf);
            while (m--)
                upos[up_hi[opbuf[m]]++] = pos;
            m = mi_defs(mi, opbuf);
            while (m--)
                upos[up_hi[opbuf[m]]++] = opbuf[m] == mi->t ? pos : pos + 1;
        }

    iv_order = realloc(iv_order, ((size_t) nvr + 1) * sizeof *iv_order);
    if (!iv_order)
        acc_error("out of memory for the machine IR");
    for (n = 0, v = 0; v != nvr; v++)
        if (iv_s[v] >= 0)
            iv_order[n++] = v;
    qsort(iv_order, (size_t) n, sizeof *iv_order, by_start);
    fixed_build(n);
    groups_build();
    grp_n = nvr;
    nheap = nact = 0;
    split_on = 1;
    for (k = 0; k != n; k++)
        heap_push(iv_order[k]);
    while (nheap) {
        int cur = heap_pop();

        for (k = 0; k < nact; k++)
            if (iv_e[act[k]] < iv_s[cur])
                act[k--] = act[--nact];
        if (!assign_free(cur) && !assign_blocked(cur)) {
            split_on = 0;
            return -1;
        }
    }
    split_on = 0;
    iv_order = realloc(iv_order, ((size_t) nvr + 1) * sizeof *iv_order);
    alloc_src = realloc(alloc_src, ((size_t) nvr + 1) * sizeof *alloc_src);
    if (!iv_order || !alloc_src)
        acc_error("out of memory for the machine IR");
    memcpy(alloc_src, copy_src, (size_t) nvr * sizeof *alloc_src);
    alloc_src_n = nvr;

    return 0;
}

/* The moves that join the parts of each value, put in: a move a record
 * of where it goes -- a block, before which of its instructions -- and of
 * what, from where to where. */
typedef struct {
    int blk, before, from, to;
} Move;

static Move *moves;
static int nmoves, moves_cap;
static unsigned char *loaded;   /* by value: its slot is read somewhere */

static void move_add(int blk, int before, int from, int to)
{
    GROW(moves, nmoves, moves_cap);
    moves[nmoves].blk = blk;
    moves[nmoves].before = before;
    moves[nmoves].from = from;
    moves[nmoves].to = to;
    nmoves++;
}

static Move *edge_moves_of;     /* for by_edge */

/* Unplaced edge moves, by index: by source, then target. */
static int by_edge(const void *x, const void *y)
{
    const Move *a = &edge_moves_of[*(const int *) x], *b = &edge_moves_of[*(const int *) y];

    if (a->blk != b->blk)
        return a->blk - b->blk;
    if (a->before != b->before)
        return b->before - a->before;

    return *(const int *) x - *(const int *) y;
}

static int by_place(const void *x, const void *y)
{
    const Move *a = x, *b = y;

    if (a->blk != b->blk)
        return a->blk - b->blk;
    if (a->before != b->before)
        return a->before - b->before;

    return a < b ? -1 : a > b;
}

/* A part's spill slot: its value's, made where first wanted -- none for
 * a constant, an address or a parameter, made again or read from its own
 * slot. */
static int slot_of(int v)
{
    int r = fam[v];

    if (vr[r].remat || vr[r].param)
        return 0;
    if (!vr[r].spill)
        vr[r].spill = new_spill(vr[r].width);

    return vr[r].spill;
}

/* The part of value `r` that `pos` is in, or -1: a binary search of its
 * parts, in order. */
static int *part_off, *part_list;

static int part_at(int r, int pos)
{
    int lo = part_off[r], hi = part_off[r + 1];

    while (lo < hi) {
        int mid = (lo + hi) / 2;

        if (iv_e[part_list[mid]] < pos)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (lo < part_off[r + 1] && iv_s[part_list[lo]] <= pos)
        return part_list[lo];

    return -1;
}

static int same_place(int x, int y)
{
    if (inmem[x] || inmem[y])
        return inmem[x] && inmem[y];

    return vr[x].preg == vr[y].preg;
}

/* The instructions a run of moves at one place makes, into `out`: the
 * stores, the registers in one parallel copy, the loads. */
static int moves_made(const Move *run, int n, MIns *out)
{
    int k, made = 0, copy = -1;

    for (k = 0; k != n; k++)
        if (!inmem[run[k].from] && inmem[run[k].to] && loaded[fam[run[k].to]]
            && slot_of(run[k].to)) {
            MIns *mi = &out[made++];

            memset(mi, 0, sizeof *mi);
            mi->op = M_STF;
            mi->a = run[k].from;
            mi->d = mi->b = mi->t = -1;
            mi->sym = -2;
            mi->imm = slot_of(run[k].to);
            mi->width = vr[run[k].from].width;
        }
    for (k = 0; k != n; k++)
        if (!inmem[run[k].from] && !inmem[run[k].to]) {
            if (copy < 0) {
                MIns *mi = &out[made++];

                copy = new_pcopy();
                memset(mi, 0, sizeof *mi);
                mi->op = M_PCOPY;
                mi->d = mi->a = mi->b = mi->t = mi->sym = -1;
                mi->imm = copy;
            }
            pcopy_add(copy, run[k].to, run[k].from);
        }
    for (k = 0; k != n; k++)
        if (inmem[run[k].from] && !inmem[run[k].to]) {
            MIns *mi = &out[made++];
            int r = fam[run[k].to];

            memset(mi, 0, sizeof *mi);
            mi->d = run[k].to;
            mi->a = mi->b = mi->t = mi->sym = -1;
            mi->width = vr[r].width;
            if (vr[r].remat) {
                mi->op = vr[r].remat;
                mi->imm = vr[r].remat_imm;
                mi->sym = vr[r].remat == M_LDSYM ? vr[r].remat_sym : -1;
                mi->imm2 = vr[r].remat == M_LDA ? vr[r].remat_sym : 0;
            } else {
                mi->op = M_LDF;
                mi->imm = vr[r].param ? vr[r].param : slot_of(run[k].to);
                mi->sym = vr[r].param ? -1 : -2;
            }
        }

    return made;
}

/* The moves for the parts made by split_scan, and the code made again
 * with them. */
static const char *resolve_bad;

static void split_resolve(void)
{
    int k, v, r, at, n, nroots = nvr, *pred_n, *npred_off, *pred_at, start;

    /* Each value's parts, in order: the list from each first part. */
    part_off = realloc(part_off, ((size_t) nvr + 2) * sizeof *part_off);
    part_list = realloc(part_list, ((size_t) nvr + 1) * sizeof *part_list);
    if (!part_off || !part_list)
        acc_error("out of memory for the machine IR");
    for (n = 0, r = 0; r != nroots; r++) {
        part_off[r] = n;
        if (fam[r] == r)
            for (v = r; v >= 0; v = child_next[v])
                if (iv_s[v] >= 0)
                    part_list[n++] = v;
    }
    part_off[nroots] = n;
    nmoves = 0;
    resolve_bad = NULL;

    /* In a block: where one part ends and the next begins, but at a
     * block's start, which the edges see to. */
    for (r = 0; r != nroots; r++)
        for (k = part_off[r]; k + 1 < part_off[r + 1]; k++) {
            int c = part_list[k], d = part_list[k + 1], x = iv_s[d];

            /* No move into a part that begins with a write of it. */
            if (same_place(c, d) || ins_idx[x >> 1] == 0 || (x & 1))
                continue;
            move_add(ins_blk[x >> 1], ins_idx[x >> 1], c, d);
        }

    /* On each edge: what each value live into a block is in at the end of
     * the block before and at the start of this one. Where one differs,
     * the moves go at the start of a block with one way in, before the
     * jump of a block with one way out, and in a block of the edge's own
     * where there are more of both. */
    pred_n = calloc((size_t) nmb + 2, sizeof *pred_n);
    if (!pred_n)
        acc_error("out of memory for the machine IR");
    for (k = 0; k != nlayout; k++)
        for (n = 0; n != mb[layout[k]].nsucc; n++)
            pred_n[mb[layout[k]].succ[n] + 2]++;
    for (k = 0; k != nmb; k++)
        pred_n[k + 2] += pred_n[k + 1];
    npred_off = pred_n;
    pred_at = malloc(((size_t) npred_off[nmb + 1] + 1) * sizeof *pred_at);
    if (!pred_at)
        acc_error("out of memory for the machine IR");
    for (k = 0; k != nlayout; k++)
        for (n = 0; n != mb[layout[k]].nsucc; n++)
            pred_at[npred_off[mb[layout[k]].succ[n] + 1]++] = layout[k];
    start = nmoves;
    for (k = 0; k != nlivein; k++) {
        int b = livein_blk[k], cin, j;

        r = livein_v[k];
        if (r >= nroots || fam[r] != r || blk_pos[b] < 0)
            continue;
        cin = part_at(r, blk_pos[b]);
        if (cin < 0)
            continue;
        for (j = npred_off[b]; j != npred_off[b + 1]; j++) {
            int pb = pred_at[j], cout;

            if (blk_pos[pb] < 0)
                continue;
            cout = part_at(r, blk_end[pb]);
            if (cout < 0 || same_place(cout, cin))
                continue;
            /* blk: the edge, for now, as its source; before: its target */
            move_add(pb, -1 - b, cout, cin);
        }
    }

    /* Where each edge's moves go. */
    for (k = start; k != nmoves; k++) {
        int pb = moves[k].blk, b = -1 - moves[k].before;
        int nin = npred_off[b + 1] - npred_off[b];

        if (nin == 1 && b != layout[0]) {
            moves[k].blk = b;
            moves[k].before = 0;
        } else if (mb[pb].nsucc == 1) {
            moves[k].before = mb[pb].n && (mb[pb].ins[mb[pb].n - 1].op == M_JMP)
                              ? mb[pb].n - 1 : mb[pb].n;
        } else {
            moves[k].before = -1 - b;   /* still the edge: a block for it */
        }
    }
    {
        /* An edge block for each edge still left: its moves, and a jump
         * on to where the edge went. Placed after the block it is from. */
        int *after_head = malloc(((size_t) nmb + 1) * sizeof *after_head), nold = nmb;
        int *after_next = NULL, nafter = 0, after_cap = 0, *after_blk = NULL;

        if (!after_head)
            acc_error("out of memory for the machine IR");
        for (k = 0; k != nmb; k++)
            after_head[k] = -1;
        /* The edges still left, each move's (source, target) taken first,
         * sorted, and a block made for each pair: nothing read from a move
         * as it is changed. */
        int *left = malloc(((size_t) (nmoves - start) + 1) * sizeof *left), nleft = 0;
        int *left_from = malloc(((size_t) nmb + 1) * sizeof *left_from);
        int prev_from = -1, prev_to = -1, e = -1;

        if (!left || !left_from)
            acc_error("out of memory for the machine IR");
        for (k = start; k != nmoves; k++)
            if (moves[k].before < 0)
                left[nleft++] = k;
        edge_moves_of = moves;
        qsort(left, (size_t) nleft, sizeof *left, by_edge);
        for (k = 0; k != nleft; k++) {
            Move *mv = &moves[left[k]];
            int pb = mv->blk, b = -1 - mv->before, j;

            if (pb != prev_from || b != prev_to) {
                MIns jmp;

                e = new_mb(-1);
                memset(&jmp, 0, sizeof jmp);
                jmp.op = M_JMP;
                jmp.d = jmp.a = jmp.b = jmp.t = jmp.sym = -1;
                jmp.imm2 = b;
                GROW(mb[e].ins, mb[e].n, mb[e].cap);
                mb[e].ins[mb[e].n++] = jmp;
                mb[e].succ[0] = b;
                mb[e].nsucc = 1;
                if (mb[pb].n && mb[pb].ins[mb[pb].n - 1].op == M_BR
                    && mb[pb].ins[mb[pb].n - 1].imm2 == b)
                    mb[pb].ins[mb[pb].n - 1].imm2 = e;
                for (j = 0; j != mb[pb].nsucc; j++)
                    if (mb[pb].succ[j] == b)
                        mb[pb].succ[j] = e;
                GROW(after_blk, nafter, after_cap);
                after_next = realloc(after_next, (size_t) after_cap * sizeof *after_next);
                if (!after_next)
                    acc_error("out of memory for the machine IR");
                after_blk[nafter] = e;
                after_next[nafter] = after_head[pb];
                after_head[pb] = nafter++;
                prev_from = pb;
                prev_to = b;
            }
            mv->blk = e;
            mv->before = 0;
        }
        free(left);
        free(left_from);
        if (nafter) {
            int *old = malloc(((size_t) nlayout + 1) * sizeof *old), nold_layout = nlayout;

            if (!old)
                acc_error("out of memory for the machine IR");
            memcpy(old, layout, (size_t) nlayout * sizeof *old);
            layout = realloc(layout, ((size_t) nlayout + nafter + 1) * sizeof *layout);
            if (!layout)
                acc_error("out of memory for the machine IR");
            nlayout = 0;
            for (k = 0; k != nold_layout; k++) {
                int a;

                layout[nlayout++] = old[k];
                if (old[k] < nold)
                    for (a = after_head[old[k]]; a >= 0; a = after_next[a])
                        layout[nlayout++] = after_blk[a];
            }
            free(old);
        }
        free(after_head);
        free(after_next);
        free(after_blk);
    }

    /* A value is stored only where a load reads it back: a part kept in
     * memory until the value is written again needs nothing there. */
    loaded = realloc(loaded, (size_t) nvr + 1);
    if (!loaded)
        acc_error("out of memory for the machine IR");
    memset(loaded, 0, (size_t) nvr + 1);
    for (k = 0; k != nmoves; k++)
        if (inmem[moves[k].from] && !inmem[moves[k].to])
            loaded[fam[moves[k].from]] = 1;

    /* The code made again, each block with its moves where they go. */
    if (nmoves)
        qsort(moves, (size_t) nmoves, sizeof *moves, by_place);
    for (k = 0; k != nmoves; ) {
        int b = moves[k].blk, j = k, made = 0, i;
        MBlock *blk;

        while (j != nmoves && moves[j].blk == b)
            j++;
        blk = &mb[b];
        for (i = k; i != j; i++)
            if (moves[i].before < 0 || moves[i].before > blk->n) {
                resolve_bad = "internal: a move with nowhere to go";
                goto out;
            }
        sp_n = 0;
        for (at = 0; at <= blk->n; at++) {
            while (k != j && moves[k].before == at) {
                int e = k;
                MIns out[3 * NPREGS + 64];

                while (e != j && moves[e].before == at)
                    e++;
                made = moves_made(moves + k, e - k, out);
                for (i = 0; i != made; i++)
                    sp_put(&out[i]);
                k = e;
            }
            if (at != blk->n)
                sp_put(&blk->ins[at]);
        }
        blk = &mb[b];
        if (sp_n > blk->cap) {
            blk->ins = realloc(blk->ins, (size_t) sp_n * sizeof *blk->ins);
            if (!blk->ins)
                acc_error("out of memory for the machine IR");
            blk->cap = sp_n;
        }
        memcpy(blk->ins, sp_out, (size_t) sp_n * sizeof *sp_out);
        blk->n = sp_n;
    }
out:
    free(pred_n);
    free(pred_at);
}

/* ------------------------------------------------------------------ */
/* checking                                                            */

/* OPTACC_MIR_DUMP: the function's machine IR, with the registers where
 * they are known. */
static void dump_vr(int v)
{
    static const char *const names[NPREGS] = {
        "a", "b", "c", "d", "e", "h", "l", "bc", "de", "hl", "iy"
    };

    if (v < 0)
        return;
    fprintf(stderr, " v%d%s", v, vr[v].width == 1 ? "b" : "");
    if (vr[v].preg >= 0)
        fprintf(stderr, "=%s", names[vr[v].preg]);
}

static void dump_mir(const char *when)
{
    static const char *const ops[NMOPS] = {
        "copy", "ldi", "ldsym", "lda", "data", "ldf", "stf", "stfi", "leaf", "ldp", "stp",
        "stpi", "stepp", "ldg", "stg", "add24", "sub24", "step24", "bytes24", "neg24", "not24",
        "alu8", "alu8i", "cmp24", "cmp24s", "cmp24si", "tst24", "cmp8", "cmp8i", "bool",
        "zext", "sext", "trunc", "helper", "br", "jmp", "ret", "pcopy", "save",
        "push", "call"
    };
    int blk, at, k;

    if (!getenv("OPTACC_MIR_DUMP"))
        return;
    fprintf(stderr, "mir of %s, %s:\n", name_text(sym_at(gl_fn)->name), when);
    for (blk = 0; blk != nmb; blk++) {
        fprintf(stderr, " m%d (ssa %d) ->", blk, mb[blk].ssa_block);
        for (k = 0; k != mb[blk].nsucc; k++)
            fprintf(stderr, " m%d", mb[blk].succ[k]);
        fprintf(stderr, "\n");
        for (at = 0; at != mb[blk].n; at++) {
            const MIns *mi = &mb[blk].ins[at];

            fprintf(stderr, "    %-7s", ops[mi->op]);
            if (mi->op == M_PCOPY) {
                for (k = 0; k != pc[mi->imm].n; k++) {
                    dump_vr(pc[mi->imm].dst[k]);
                    fprintf(stderr, " <-");
                    dump_vr(pc[mi->imm].src[k]);
                    fprintf(stderr, ";");
                }
            } else {
                dump_vr(mi->d);
                if (mi->d >= 0)
                    fprintf(stderr, " <-");
                dump_vr(mi->a);
                dump_vr(mi->b);
                if (mi->t >= 0) {
                    fprintf(stderr, " clobbers");
                    dump_vr(mi->t);
                }
                fprintf(stderr, "  imm %d imm2 %d%s", mi->imm, mi->imm2,
                        mi->sym == -2 ? " (spill)" : "");
            }
            fprintf(stderr, "\n");
        }
    }
}

/* Every operand in its class, and no two values live at once sharing a
 * unit: run after allocation, over the allocation itself. */
static const char *verify(void)
{
    int k, j, n = 0, nactive = 0, active[NPREGS + 8], blk, at;

    for (blk = 0; blk != nmb; blk++)
        for (at = 0; at != mb[blk].n; at++) {
            const MIns *mi = &mb[blk].ins[at];

            if (mi->op == M_COPY && vr[mi->d].width != vr[mi->a].width)
                return "internal: a copy from one width to another";
        }

    /* The intervals in order, with those live at each start: none sharing
     * a unit, each in its class, none across what clobbers its register. */
    clob_build();
    for (k = 0; k != nvr; k++)
        if (iv_s[k] >= 0)
            iv_order[n++] = k;
    qsort(iv_order, (size_t) n, sizeof *iv_order, by_start);
    for (k = 0; k != n; k++) {
        int v = iv_order[k];

        if (vr[v].preg < 0 && splitting && v < va_cap && inmem[v])
            continue;                   /* a part in memory */
        if (vr[v].preg < 0 || !(vr[v].cls & PB(vr[v].preg))) {
            clob_free();
            return "internal: a register outside its class";
        }
        if (preg_units[vr[v].preg] & clob_or(iv_s[v], iv_e[v] - 1)) {
            clob_free();
            return "internal: a register a clobber takes";
        }
        for (j = 0; j < nactive; j++)
            if (iv_e[active[j]] < iv_s[v])
                active[j--] = active[--nactive];
        for (j = 0; j != nactive; j++)
            if ((preg_units[vr[active[j]].preg] & preg_units[vr[v].preg])
                && !(same_value(active[j], v) && vr[active[j]].preg == vr[v].preg)) {
                clob_free();
                return "internal: two values sharing a register";
            }
        if (nactive == NPREGS + 8) {
            clob_free();
            return "internal: more live than registers";
        }
        active[nactive++] = v;
    }
    clob_free();

    return NULL;
}

/* After the parts are joined: each read finds its register holding what
 * it reads, on every path to it. Which value each register unit holds,
 * and which part's value each spill slot, followed through the code,
 * each block from what all the blocks before it agree on -- an unknown
 * or a disagreement is a failure. Passes over the blocks until nothing
 * changes, a handful for the deepest loops: each linear in the code. A
 * register holds a value: a copy that shares it with what it copies, as
 * the scan lets one, holds the same. */
#define H_TOP (-3)              /* not reached yet */
#define H_ANY (-2)              /* the paths disagree, or nothing known */
#define H_UNDEF (-4)            /* as the function begins: what a value
                                 * read before it is set may be */

static int *hold_in, *hold_out, *slot_in, *slot_out;
static unsigned long long *set_out;     /* by block: the read-before-set
                                         * values set on some path to its
                                         * end, a bit each */
static int *undef_bit;                  /* by value: its bit, or -1 */

/* What a register holds, as the checker names it: the value a part is of,
 * and a copy as what it copies. */
static int value_key(int v)
{
    int k = splitting && v < va_cap ? fam[v] : v;

    if (!splitting)
        return value_of(k);

    return k < alloc_src_n && alloc_src[k] >= 0 ? alloc_src[k] : k;
}

static int family_of(int v)
{
    return splitting && v < va_cap ? fam[v] : v;
}
/* Two paths' knowledge of one place, met: what one has and the other
 * has not reached yet, or has as it was when the function began. */
static int npruned;              /* moves check_joined took out */

/* Whether what is in a place, undefined as the function began, will do
 * for a read of `v`: where `v` is read before it is set, and no path here
 * has set it yet. */
static int undef_here(int h, int v, unsigned long long set_now)
{
    int bit;

    if (h != H_UNDEF || v >= nvr)
        return 0;
    bit = undef_bit[family_of(v)];

    return bit >= 0 && !(set_now >> bit & 1);
}

/* What the paths into a block agree a place holds, `init` the first
 * block's own: one path's value where the others have not been reached;
 * undefined only where every path has it so -- or, met with a value of
 * one read before it is set, that value, where the paths still undefined
 * have not set it; pruning, never. */
static int check_merge(int init, const int *out, int width, int at, int from,
                       int to, const int *pred, int prune)
{
    int result = init == H_UNDEF ? H_TOP : init, any_undef = init == H_UNDEF, j, bit;
    unsigned long long undef_sets = 0;

    for (j = from; j != to; j++) {
        int h = out[pred[j] * width + at];

        if (h == H_TOP)
            continue;
        if (h == H_UNDEF) {
            any_undef = 1;
            undef_sets |= set_out[pred[j]];
            continue;
        }
        result = result == H_TOP || result == h ? h : H_ANY;
    }
    if (!any_undef)
        return result;
    if (prune)
        return H_ANY;
    if (result == H_TOP)
        return H_UNDEF;
    if (result < 0 || result >= nvr)
        return H_ANY;
    bit = undef_bit[result];

    return bit >= 0 && !(undef_sets >> bit & 1) ? result : H_ANY;
}

/* With `prune`, the same walk takes out the moves of what is already
 * where they move it -- on every path, strictly: a value read before it is
 * set is not there on the path that does not set it -- and says nothing;
 * without, it checks. */
static const char *check_joined(int prune)
{
    int nunits = U_F + 1, nslot = nspills + 1, pass, k, changed = 1;
    int *pred_off = calloc((size_t) nmb + 2, sizeof *pred_off), *pred_at;
    int hold[U_F + 1], saved[U_F + 1], *slot = malloc(((size_t) nslot + 1) * sizeof *slot);
    unsigned long long set_now = 0;
    int nbits = 0;
    const char *bad = NULL;

    hold_in = realloc(hold_in, ((size_t) nmb + 1) * nunits * sizeof *hold_in);
    hold_out = realloc(hold_out, ((size_t) nmb + 1) * nunits * sizeof *hold_out);
    slot_in = realloc(slot_in, ((size_t) nmb + 1) * nslot * sizeof *slot_in);
    slot_out = realloc(slot_out, ((size_t) nmb + 1) * nslot * sizeof *slot_out);
    set_out = realloc(set_out, ((size_t) nmb + 1) * sizeof *set_out);
    undef_bit = realloc(undef_bit, ((size_t) nvr + 1) * sizeof *undef_bit);
    if (!pred_off || !slot || !hold_in || !hold_out || !slot_in || !slot_out
        || !set_out || !undef_bit)
        acc_error("out of memory for the machine IR");

    /* The values read before they are set on some path: live into the
     * function's first block. Reading what is there as it begins is what
     * such a read is -- on a path that has not set it yet. A bit each, for
     * the first 64 of them; the rest are held to what they are. */
    for (k = 0; k != nvr; k++)
        undef_bit[k] = -1;
    for (k = 0; k != nlivein; k++)
        if (livein_blk[k] == layout[0] && livein_v[k] < nvr
            && undef_bit[livein_v[k]] < 0 && nbits < 64)
            undef_bit[livein_v[k]] = nbits++;
    for (k = 0; k != nmb; k++)
        set_out[k] = 0;
    for (k = 0; k != nlayout; k++) {
        int n;

        for (n = 0; n != mb[layout[k]].nsucc; n++)
            pred_off[mb[layout[k]].succ[n] + 2]++;
    }
    for (k = 0; k != nmb; k++)
        pred_off[k + 2] += pred_off[k + 1];
    pred_at = malloc(((size_t) pred_off[nmb + 1] + 1) * sizeof *pred_at);
    if (!pred_at)
        acc_error("out of memory for the machine IR");
    for (k = 0; k != nlayout; k++) {
        int n;

        for (n = 0; n != mb[layout[k]].nsucc; n++)
            pred_at[pred_off[mb[layout[k]].succ[n] + 1]++] = layout[k];
    }
    for (k = 0; k != (nmb + 1) * nunits; k++)
        hold_out[k] = H_TOP;
    for (k = 0; k != (nmb + 1) * nslot; k++)
        slot_out[k] = H_TOP;

    /* To a fixed point; then once more, saying what is wrong. */
    for (pass = 0; !bad && pass != 66; pass++) {
        int report = !changed || pass == 65;

        changed = 0;
        for (k = 0; k != nlayout && !bad; k++) {
            int b = layout[k], u, j, at, pos, reached;

            /* What the blocks before agree on. */
            for (u = 0; u != nunits; u++)
                hold[u] = k == 0 ? (prune ? H_ANY : H_UNDEF) : H_TOP;
            for (j = 0; j != nslot; j++)
                slot[j] = k == 0 ? (prune ? H_ANY : H_UNDEF) : H_TOP;
            reached = k == 0;
            set_now = 0;
            for (j = pred_off[b]; j != pred_off[b + 1]; j++)
                if (hold_out[pred_at[j] * nunits] != H_TOP) {
                    reached = 1;
                    set_now |= set_out[pred_at[j]];
                }
            for (u = 0; u != nunits; u++)
                hold[u] = check_merge(hold[u], hold_out, nunits, u, pred_off[b],
                                      pred_off[b + 1], pred_at, prune);
            for (j = 0; j != nslot; j++)
                slot[j] = check_merge(slot[j], slot_out, nslot, j, pred_off[b],
                                      pred_off[b + 1], pred_at, prune);
            if (!reached)
                continue;               /* nothing comes here, so far */
            for (u = 0; u != nunits; u++)
                if (hold[u] == H_TOP)
                    hold[u] = H_ANY;
            for (j = 0; j != nslot; j++)
                if (slot[j] == H_TOP)
                    slot[j] = H_ANY;

            pos = blk_pos[b];
            for (at = 0; at != mb[b].n; at++, pos += 2) {
                const MIns *mi = &mb[b].ins[at];
                int n, q, uses[MAX_PCOPY + 8], defs[MAX_PCOPY + 8];
                int src_here[MAX_PCOPY + 8];
                unsigned cl;

                opbuf_fit(mi_nops(mi));
                n = mi_uses(mi, opbuf);
                for (q = 0; q != n && q != MAX_PCOPY + 8; q++)
                    uses[q] = opbuf[q];
                for (q = 0; q != n; q++) {
                    int v = uses[q], p = vr[v].preg, here = 1;

                    if (p < 0)
                        continue;
                    for (u = 0; u != nunits; u++)
                        if ((preg_units[p] & UB(u)) && hold[u] != value_key(v)
                            && !undef_here(hold[u], v, set_now))
                            here = 0;
                    if (q < MAX_PCOPY + 8)
                        src_here[q] = here;

                    /* A move of a value to where else it lives -- a store
                     * to its slot, a copy to another part of it -- reads
                     * no use of it: what it moves is checked where read. */
                    if ((mi->op == M_STF && mi->sym == -2)
                        || (mi->op == M_PCOPY
                            && family_of(pc[mi->imm].dst[q]) == family_of(v)))
                        continue;
                    for (u = 0; u != nunits; u++)
                        if (report && !prune && (preg_units[p] & UB(u)) && hold[u] != value_key(v)
                            && !undef_here(hold[u], v, set_now)) {
                            static char why[96];

                            snprintf(why, sizeof why,
                                     "internal: m%d's instruction %d reads v%d, not there",
                                     b, at, v);
                            bad = why;
                        }
                }
                /* A reload: the slot holds a part of the same value. */
                if (report && !prune && mi->op == M_LDF && mi->sym == -2 && mi->d >= 0
                    && (mi->imm >= nslot || (slot[mi->imm] != family_of(mi->d)
                                             && !undef_here(slot[mi->imm], mi->d, set_now)))) {
                    static char why[96];

                    snprintf(why, sizeof why,
                             "internal: m%d's instruction %d reloads v%d's slot, not stored",
                             b, at, mi->d);
                    bad = why;
                }
                /* A move of what is there already: nothing, taken out. */
                if (prune && report) {
                    MIns *w = &mb[b].ins[at];

                    if (w->op == M_STF && w->sym == -2 && w->imm < nslot
                        && slot[w->imm] == family_of(w->a) && src_here[0]) {
                        w->op = M_DEAD;
                        npruned++;
                        continue;
                    }
                    if (w->op == M_LDF && w->sym == -2 && vr[w->d].preg >= 0) {
                        int all = 1;

                        for (u = 0; u != nunits; u++)
                            if ((preg_units[vr[w->d].preg] & UB(u))
                                && hold[u] != value_key(w->d))
                                all = 0;
                        if (all) {
                            w->op = M_DEAD;
                            npruned++;
                            continue;
                        }
                    }
                    if (w->op == M_PCOPY) {
                        PCopy *pcp = &pc[w->imm];
                        int e2, put = 0;

                        for (e2 = 0; e2 != pcp->n; e2++) {
                            int d2 = pcp->dst[e2], keep = 1;

                            if (family_of(d2) == family_of(pcp->src[e2]) && vr[d2].preg >= 0
                                && vr[d2].preg == vr[pcp->src[e2]].preg)
                                keep = 0;       /* the same register */
                            if (keep) {
                                pcp->dst[put] = pcp->dst[e2];
                                pcp->src[put++] = pcp->src[e2];
                            } else {
                                npruned++;
                            }
                        }
                        pcp->n = put;
                    }
                }
                if (mi->op == M_STF && mi->sym == -2 && mi->a >= 0 && mi->imm < nslot)
                    slot[mi->imm] = src_here[0] ? family_of(mi->a) : H_ANY;

                /* A call: the pairs pushed at its save come back as they
                 * were then; anything else the callee may have changed. */
                if (mi->op == M_SAVE)
                    for (u = 0; u != nunits; u++)
                        saved[u] = hold[u];
                if (mi->op == M_CALL)
                    for (u = 0; u != nunits; u++) {
                        unsigned bit = UB(u);
                        int kept = ((mi->imm2 & 1) && (preg_units[P_BC] & bit))
                                   || ((mi->imm2 & 2) && (preg_units[P_DE] & bit))
                                   || ((mi->imm2 & 4) && (preg_units[P_IY] & bit));

                        hold[u] = kept ? saved[u] : H_ANY;
                    }

                /* What it clobbers, then what it writes. */
                cl = pos < npos ? clob[pos] : 0;
                if (mi->op == M_CALL)
                    cl |= UB(U_A) | preg_units[P_HL];
                if (mi->op == M_HELPER)
                    cl |= UB(U_A);
                for (u = 0; u != nunits; u++)
                    if (cl & UB(u))
                        hold[u] = H_ANY;
                n = mi_defs(mi, opbuf);
                for (q = 0; q != n && q != MAX_PCOPY + 8; q++)
                    defs[q] = opbuf[q];
                for (q = 0; q != n; q++) {
                    int v = defs[q], p = vr[v].preg, key = value_key(v), moved;

                    /* A move of a value from one place to another keeps
                     * the rest of where it is; anything else writes it
                     * anew, and every other place holding it is stale. */
                    moved = (mi->op == M_COPY && value_key(mi->a) == key)
                            || (mi->op == M_PCOPY && value_key(pc[mi->imm].src[q]) == key)
                            || (mi->op == M_LDF && mi->sym == -2)
                            || v == mi->t;
                    if (!moved && undef_bit[family_of(v)] >= 0)
                        set_now |= 1ULL << undef_bit[family_of(v)];
                    if (!moved) {
                        int m;

                        for (u = 0; u != nunits; u++)
                            if (hold[u] == key)
                                hold[u] = H_ANY;
                        for (m = 0; m != nslot; m++)
                            if (slot[m] == family_of(v))
                                slot[m] = H_ANY;
                    }
                    if (p < 0)
                        continue;
                    if (mi->op == M_PCOPY && q < MAX_PCOPY + 8
                        && family_of(pc[mi->imm].src[q]) == family_of(v)
                        && !src_here[q])
                        key = H_ANY;            /* moved, but not what it is */
                    for (u = 0; u != nunits; u++)
                        if (preg_units[p] & UB(u))
                            hold[u] = v == mi->t ? H_ANY : key;
                }
            }
            if (set_out[b] != set_now) {
                set_out[b] = set_now;
                changed = 1;
            }
            for (u = 0; u != nunits; u++)
                if (hold_out[b * nunits + u] != hold[u]) {
                    hold_out[b * nunits + u] = hold[u];
                    changed = 1;
                }
            for (j = 0; j != nslot; j++)
                if (slot_out[b * nslot + j] != slot[j]) {
                    slot_out[b * nslot + j] = slot[j];
                    changed = 1;
                }
        }
        if (report)
            break;
    }
    free(pred_off);
    free(pred_at);
    free(slot);

    /* What was taken out, taken out of the blocks. */
    if (npruned)
        for (k = 0; k != nmb; k++) {
            int at, put = 0;

            for (at = 0; at != mb[k].n; at++)
                if (mb[k].ins[at].op != M_DEAD
                    && !(mb[k].ins[at].op == M_PCOPY && pc[mb[k].ins[at].imm].n == 0))
                    mb[k].ins[put++] = mb[k].ins[at];
            mb[k].n = put;
        }

    return bad;
}

/* The units busy at each parallel copy, for a cycle of its bytes to go
 * round: the intervals live across it, recorded in the copy. */
static void pcopy_busy(void)
{
    int k, n = 0, j, nactive = 0, active[NPREGS + 8], next = 0, b, at, pos = 0;
    int save_pos = 0;
    MIns *save = NULL;

    for (k = 0; k != nvr; k++)
        if (iv_s[k] >= 0)
            iv_order[n++] = k;
    qsort(iv_order, (size_t) n, sizeof *iv_order, by_start);
    for (b = 0; b != nlayout; b++) {
        MBlock *blk = &mb[layout[b]];

        for (at = 0; at != blk->n; at++, pos += 2) {
            unsigned busy = 0;

            while (next < n && iv_s[iv_order[next]] <= pos) {
                if (nactive < NPREGS + 8)
                    active[nactive++] = iv_order[next];
                next++;
            }
            for (j = 0; j < nactive; j++)
                if (iv_e[active[j]] < pos)
                    active[j--] = active[--nactive];
            if (blk->ins[at].op == M_SAVE) {
                save = &blk->ins[at];
                save_pos = pos;
            }
            if (blk->ins[at].op == M_CALL) {
                for (j = 0; j != nactive; j++)
                    if (vr[active[j]].preg >= 0 && iv_s[active[j]] < save_pos
                        && iv_e[active[j]] > pos + 1)
                        busy |= preg_units[vr[active[j]].preg];
                blk->ins[at].imm2 = (busy & preg_units[P_BC] ? 1 : 0)
                                    | (busy & preg_units[P_DE] ? 2 : 0)
                                    | (busy & preg_units[P_IY] ? 4 : 0);
                if (save)
                    save->imm2 = blk->ins[at].imm2;
                save = NULL;
                continue;
            }
            if (blk->ins[at].op != M_PCOPY)
                continue;
            for (j = 0; j != nactive; j++)
                if (vr[active[j]].preg >= 0 && iv_e[active[j]] > pos + 1)
                    busy |= preg_units[vr[active[j]].preg];
            blk->ins[at].imm2 = (int) busy;
        }
    }
}

/* ------------------------------------------------------------------ */
/* emitting                                                            */

static int *spill_off;          /* by spill: its displacement from IX */
static int *mb_addr;            /* by machine block: where it starts, or -1 */
static int *pend_hole, *pend_next, *pend_head, npend, pend_cap;

static int code8(int preg) { return r8_code[preg]; }

/* The second byte of the ld rr, (ix+d) family, and the like, by pair. */
static int pair_op(int preg, int bc, int de, int hl, int iy)
{
    switch (preg) {
    case P_BC: return bc;
    case P_DE: return de;
    case P_HL: return hl;
    }

    return iy;
}

static int disp_of(const MIns *mi)
{
    return mi->sym == -2 ? spill_off[mi->imm - 1] : mi->imm;
}

static int pr(int v)
{
    return v >= 0 ? vr[v].preg : -1;
}

static void push_pair(int p)
{
    if (p == P_IY)
        out_byte2(0xfd, 0xe5);
    else
        out_byte(pair_op(p, 0xc5, 0xd5, 0xe5, 0));
}

static void pop_pair(int p)
{
    if (p == P_IY)
        out_byte2(0xfd, 0xe1);
    else
        out_byte(pair_op(p, 0xc1, 0xd1, 0xe1, 0));
}

static void move8(int d, int s)
{
    if (d != s)
        out_byte(0x40 | code8(d) << 3 | code8(s));
}

static void move24(int d, int s)
{
    if (d == s)
        return;
    push_pair(s);
    pop_pair(d);
}

static void imm24(int value)
{
    out_byte(value & 0xff);
    out_byte((value >> 8) & 0xff);
    out_byte((value >> 16) & 0xff);
}

static void ld_pair_imm(int p, int value)
{
    if (p == P_IY)
        out_byte(0xfd);
    out_byte(pair_op(p, 0x01, 0x11, 0x21, 0x21));
    imm24(value);
}

/* nn with the link adding a symbol's address to it. */
static void sym_nn(int sym, int off)
{
    imm24(off);
    fixup_add(sym, out_here() - ACC_INT_SIZE);
}

/* An address constant's nn: a string's address where it is now, which the
 * link moves with the image; or a static's offset into the bss, which the
 * link adds the bss's start to. */
static void addrc_nn(int value, int kind)
{
    if (kind == VAL_ADDR)
        out_reloc(out_here());
    else
        gen_bss_fixup(out_here());
    out_word24(kind == VAL_ADDR ? ssa_moved_at(value) : value);
}

/* A load's or a store's nn: a global's, or an address constant's. */
static void global_nn(const MIns *mi)
{
    if (mi->sym >= 0)
        sym_nn(mi->sym, mi->imm);
    else
        addrc_nn(mi->imm, mi->imm2);
}

static void jump_to_block(int cc, int to)
{
    if (mb_addr[to] >= 0) {
        if (cc == JP_ANY)
            gen_jump_to(mb_addr[to]);
        else
            gen_jump_cc_to(cc, mb_addr[to]);
        return;
    }
    GROW(pend_hole, npend, pend_cap);
    pend_next = realloc(pend_next, (size_t) pend_cap * sizeof *pend_next);
    if (!pend_next)
        acc_error("out of memory for the machine IR");
    pend_hole[npend] = cc == JP_ANY ? gen_jump() : jump_op(cc);
    pend_next[npend] = pend_head[to];
    pend_head[to] = npend++;
}

/* A parallel copy: the pairs through the stack, every source pushed before
 * any destination is popped; the bytes in an order that writes none before
 * it is read, a cycle broken through a free byte register. `emit` clear:
 * whether it can be made, without making it. */

static int emit_pcopy(const PCopy *p, unsigned busy, int emit)
{
    int k, left, moved, src[MAX_PCOPY], done[MAX_PCOPY];

    if (p->n > MAX_PCOPY)
        return 0;
    for (k = 0; k != p->n; k++) {
        src[k] = pr(p->src[k]);
        busy |= preg_units[src[k]] | preg_units[pr(p->dst[k])];
        done[k] = vr[p->src[k]].width == 3 || src[k] == pr(p->dst[k]);
    }
    if (emit)
        for (k = 0; k != p->n; k++)
            if (vr[p->src[k]].width == 3 && src[k] != pr(p->dst[k]))
                push_pair(src[k]);
    do {
        moved = 0;
        left = 0;
        for (k = 0; k != p->n; k++) {
            int j, blocked = 0;

            if (done[k])
                continue;
            left++;
            for (j = 0; j != p->n; j++)
                if (!done[j] && j != k && src[j] == pr(p->dst[k]))
                    blocked = 1;
            if (blocked)
                continue;
            if (emit)
                move8(pr(p->dst[k]), src[k]);
            done[k] = 1;
            moved = 1;
        }
        if (left && !moved) {
            /* A cycle: one source into a free byte, read from there. */
            int free_r = -1, r;

            for (r = P_A; r <= P_L && free_r < 0; r++)
                if (!(preg_units[r] & busy))
                    free_r = r;
            if (free_r < 0)
                return 0;
            for (k = 0; k != p->n; k++)
                if (!done[k])
                    break;
            if (emit)
                move8(free_r, src[k]);
            src[k] = free_r;
            busy |= preg_units[free_r];
        }
    } while (left);
    if (emit)
        for (k = p->n - 1; k >= 0; k--)
            if (vr[p->src[k]].width == 3 && pr(p->src[k]) != pr(p->dst[k]))
                pop_pair(pr(p->dst[k]));

    return 1;
}

/* A byte read into A only to be copied on, or copied into A only to be
 * written: through HL or IY, one instruction, ld r, (hl) or ld (hl), r
 * -- A not written, which nothing else was to read. Whether made. */
static int byte_via_a(const MIns *mi, const MIns *next)
{
    const MIns *mem = mi->op == M_LDP ? mi : next;
    int base = pr(mem->a), r;

    if ((mi->op != M_LDP && (mi->op != M_COPY || next->op != M_STP))
        || mem->width != 1 || mem->imm || (base != P_HL && base != P_IY))
        return 0;
    if (mi->op == M_LDP) {
        if (next->op != M_COPY || next->a != mi->d || pr(next->d) == P_A)
            return 0;
        r = pr(next->d);
        if (base == P_IY)
            out_byte3(0xfd, 0x46 | code8(r) << 3, 0);
        else
            out_byte(0x46 | code8(r) << 3);
        return 1;
    }
    if (mi->op != M_COPY || next->op != M_STP || next->b != mi->d
        || vr[mi->d].width != 1 || pr(mi->a) == P_A)
        return 0;
    r = pr(mi->a);
    if (base == P_IY)
        out_byte3(0xfd, 0x70 | code8(r), 0);
    else
        out_byte(0x70 | code8(r));

    return 1;
}

/* Whether the flags are those of A's value, as or a, a would make them:
 * after &, | or ^ into A, until anything else is made. */
static int flags_of_a;

static void make_mi(const MIns *mi, int next_blk, int falls_to)
{
    int d = pr(mi->d), a = pr(mi->a), b = pr(mi->b), k;
    int logic = (mi->op == M_ALU8 || mi->op == M_ALU8I)
                && (mi->imm == TK_AMP || mi->imm == TK_PIPE || mi->imm == TK_CARET);

    if (mi->op == M_CMP8I && (mi->imm2 & 0xff) == 0 && a == P_A && flags_of_a)
        return;                         /* made already */
    if (!(mi->op == M_COPY && d == a))
        flags_of_a = logic;
    switch (mi->op) {
    case M_COPY:
        if (vr[mi->d].width == 1)
            move8(d, a);
        else
            move24(d, a);
        return;
    case M_LDI:
        if (vr[mi->d].width == 1)
            out_byte2(0x06 | code8(d) << 3, mi->imm & 0xff);
        else
            ld_pair_imm(d, mi->imm);
        return;
    case M_LDSYM:
        if (d == P_IY)
            out_byte(0xfd);
        out_byte(pair_op(d, 0x01, 0x11, 0x21, 0x21));
        sym_nn(mi->sym, mi->imm);
        return;
    case M_LDA:
        if (d == P_IY)
            out_byte(0xfd);
        out_byte(pair_op(d, 0x01, 0x11, 0x21, 0x21));
        addrc_nn(mi->imm, mi->imm2);
        return;
    case M_DATA: {
        const GenRec *rec = insns[mi->ssa].rec;
        int bytes = (int) rec->arg[1];

        ssa_moved_add((int) rec->ret, bytes, gen_data(gl_kept(rec->arg[0]), bytes));
        return;
    }
    case M_LDF:
        if (mi->width == 1)
            out_byte3(0xdd, 0x46 | code8(d) << 3, disp_of(mi) & 0xff);
        else
            out_byte3(0xdd, pair_op(d, 0x07, 0x17, 0x27, 0x31), disp_of(mi) & 0xff);
        return;
    case M_STF:
        if (mi->width == 1)
            out_byte3(0xdd, 0x70 | code8(a), disp_of(mi) & 0xff);
        else
            out_byte3(0xdd, pair_op(a, 0x0f, 0x1f, 0x2f, 0x3e), disp_of(mi) & 0xff);
        return;
    case M_STFI:
        out_byte3(0xdd, 0x36, disp_of(mi) & 0xff);
        out_byte(mi->imm2 & 0xff);
        return;
    case M_LEAF:
        out_byte3(0xed, pair_op(d, 0x02, 0x12, 0x22, 0x55), mi->imm & 0xff);
        return;
    case M_LDP:
        if (a == P_IY) {
            if (mi->width == 1)
                out_byte3(0xfd, 0x46 | code8(d) << 3, mi->imm & 0xff);
            else                                        /* ld iy, (iy+d): fd 37 */
                out_byte3(0xfd, pair_op(d, 0x07, 0x17, 0x27, 0x37), mi->imm & 0xff);
        } else if (mi->width == 1 && a != P_HL) {
            out_byte(a == P_BC ? 0x0a : 0x1a);          /* ld a, (bc) or (de) */
        } else if (mi->width == 1) {
            out_byte(0x46 | code8(d) << 3);             /* ld r, (hl) */
        } else {
            out_byte2(0xed, pair_op(d, 0x07, 0x17, 0x27, 0x31));
        }
        return;
    case M_STP:
        if (a == P_IY) {
            if (mi->width == 1)
                out_byte3(0xfd, 0x70 | code8(b), mi->imm & 0xff);
            else                                        /* ld (iy+d), iy: fd 3f */
                out_byte3(0xfd, pair_op(b, 0x0f, 0x1f, 0x2f, 0x3f), mi->imm & 0xff);
        } else if (mi->width == 1 && a != P_HL) {
            out_byte(a == P_BC ? 0x02 : 0x12);          /* ld (bc) or (de), a */
        } else if (mi->width == 1) {
            out_byte(0x70 | code8(b));                  /* ld (hl), r */
        } else {
            out_byte2(0xed, pair_op(b, 0x0f, 0x1f, 0x2f, 0x3e));
        }
        return;
    case M_STPI:
        if (a == P_IY) {
            out_byte3(0xfd, 0x36, mi->imm & 0xff);
            out_byte(mi->imm2 & 0xff);
        } else {
            out_byte2(0x36, mi->imm2 & 0xff);
        }
        return;
    case M_STEPP:
        if (a == P_IY)
            out_byte3(0xfd, mi->imm2 < 0 ? 0x35 : 0x34, mi->imm & 0xff);
        else
            out_byte(mi->imm2 < 0 ? 0x35 : 0x34);       /* inc or dec (hl) */
        return;
    case M_LDG:
        if (mi->width == 1)
            out_byte(0x3a);                             /* ld a, (nn) */
        else if (d == P_HL)
            out_byte(0x2a);
        else if (d == P_IY)
            out_byte2(0xfd, 0x2a);
        else
            out_byte2(0xed, d == P_BC ? 0x4b : 0x5b);
        global_nn(mi);
        return;
    case M_STG:
        if (mi->width == 1)
            out_byte(0x32);                             /* ld (nn), a */
        else if (a == P_HL)
            out_byte(0x22);
        else if (a == P_IY)
            out_byte2(0xfd, 0x22);
        else
            out_byte2(0xed, a == P_BC ? 0x43 : 0x53);
        global_nn(mi);
        return;
    case M_ADD24:
        out_byte(pair_op(b, 0x09, 0x19, 0x29, 0));      /* add hl, rr */
        return;
    case M_SUB24: case M_CMP24:
        out_byte(0xb7);                                 /* or a, a */
        out_byte2(0xed, pair_op(b, 0x42, 0x52, 0x62, 0));   /* sbc hl, rr */
        return;
    case M_CMP24S: {
        /* The difference's sign is the answer unless it overflowed, and
         * the other way round where it did: the sign into the carry, and
         * the carry turned over on an overflow -- add hl, hl keeps P/V. */
        int over;

        out_byte(0xb7);
        out_byte2(0xed, 0x52);                          /* sbc hl, de */
        out_byte(0x29);                                 /* add hl, hl */
        over = jump_op(0xe2);                           /* jp po */
        out_byte(0x3f);                                 /* ccf */
        patch_to_here(over);
        return;
    }
    case M_CMP24SI:
        if (!mi->imm2) {
            out_byte(0x29);                             /* add hl, hl */
            return;
        }
        ld_pair_imm(P_DE, 0x800000);
        out_byte(0x19);                                 /* add hl, de */
        ld_pair_imm(P_DE, (mi->imm2 + 0x800000) & 0xffffff);
        out_byte(0xb7);
        out_byte2(0xed, 0x52);                          /* sbc hl, de */
        return;
    case M_TST24:
        out_byte(0x09);                                 /* add hl, bc */
        out_byte(0xb7);
        out_byte2(0xed, 0x42);                          /* sbc hl, bc */
        return;
    case M_STEP24:
        if (d != a)
            move24(d, a);
        for (k = 0; k != (mi->imm < 0 ? -mi->imm : mi->imm); k++) {
            if (d == P_IY)
                out_byte(0xfd);
            out_byte(mi->imm < 0 ? pair_op(d, 0x0b, 0x1b, 0x2b, 0x2b)
                                 : pair_op(d, 0x03, 0x13, 0x23, 0x23));
        }
        return;
    case M_NEG24: case M_NOT24:
        out_byte(0xeb);                                 /* ex de, hl */
        out_byte(mi->op == M_NEG24 ? 0xb7 : 0x37);      /* or a / scf */
        out_byte2(0xed, 0x62);                          /* sbc hl, hl */
        out_byte(0xb7);
        out_byte2(0xed, 0x52);                          /* sbc hl, de */
        return;
    case M_ALU8: {
        int base = mi->imm == TK_PLUS ? 0x80 : mi->imm == TK_MINUS ? 0x90
                   : mi->imm == TK_AMP ? 0xa0 : mi->imm == TK_CARET ? 0xa8 : 0xb0;

        out_byte(base | code8(b));
        return;
    }
    case M_BYTES24: {
        static const int high_of[] = { [P_BC] = P_B, [P_DE] = P_D, [P_HL] = P_H };
        int alu = mi->imm == TK_AMP ? 0xe6 : mi->imm == TK_CARET ? 0xee : 0xf6;

        if (d != a)
            move24(d, a);
        for (k = 0; k != 2; k++) {
            int r = k ? high_of[d] : low_of(d), c = (mi->imm2 >> (8 * k)) & 0xff;

            switch (byte_op_cost(mi->imm, c)) {
            case 0:
                break;
            case 2:
                out_byte2(0x06 | code8(r) << 3, c);     /* ld r, n */
                break;
            default:
                out_byte(0x78 | code8(r));              /* ld a, r */
                out_byte2(alu, c);
                out_byte(0x47 | code8(r) << 3);         /* ld r, a */
            }
        }
        return;
    }
    case M_ALU8I: {
        int op = mi->imm == TK_PLUS ? 0xc6 : mi->imm == TK_MINUS ? 0xd6
                 : mi->imm == TK_AMP ? 0xe6 : mi->imm == TK_CARET ? 0xee : 0xf6;

        out_byte2(op, mi->imm2 & 0xff);
        return;
    }
    case M_CMP8:
        out_byte(0xb8 | code8(b));
        return;
    case M_CMP8I:
        if ((mi->imm2 & 0xff) == 0)
            out_byte(0xb7);                             /* or a, a */
        else
            out_byte2(0xfe, mi->imm2 & 0xff);
        return;
    case M_BOOL:
        if (vr[mi->d].width == 1) {
            out_byte2(0x06 | code8(d) << 3, 0);
            out_byte2(0x20 + ((mi->imm ^ 0x08) & 0x18), 1);
            out_byte(0x04 | code8(d) << 3);             /* inc r */
        } else {
            ld_pair_imm(d, 0);
            out_byte2(0x20 + ((mi->imm ^ 0x08) & 0x18), 1);
            out_byte(pair_op(d, 0x03, 0x13, 0x23, 0));  /* inc rr */
        }
        return;
    case M_ZEXT:
        if (d == P_HL) {
            out_byte(0xb7);
            out_byte2(0xed, 0x62);                      /* sbc hl, hl */
        } else {
            ld_pair_imm(d, 0);
        }
        move8(low_of(d), P_A);
        return;
    case M_SEXT:
        out_byte(0x6f);                                 /* ld l, a */
        out_byte2(0xcb, 0x05);                          /* rlc l */
        out_byte2(0xed, 0x62);                          /* sbc hl, hl */
        out_byte(0x6f);
        return;
    case M_TRUNC:
        move8(d, low_of(a));
        return;
    case M_HELPER:
        rt_call(mi->imm);
        return;
    case M_SAVE:
        if (mi->imm2 & 4)
            push_pair(P_IY);
        if (mi->imm2 & 1)
            push_pair(P_BC);
        if (mi->imm2 & 2)
            push_pair(P_DE);
        return;
    case M_PUSH:
        push_pair(pr(mi->a));
        return;
    case M_CALL: {
        const Sym *callee = sym_at(mi->sym);
        int slot;

        if (sym_flags(mi->sym) & SYMF_DEFINED) {
            want(mi->sym);
            out_reloc(out_here() + 1);
            out_opcode24(0xcd, callee->val);            /* call nn */
        } else {
            out_opcode24(0xcd, 0);
            fixup_add(mi->sym, out_here() - ACC_INT_SIZE);
        }
        for (slot = 0; slot != mi->imm; slot++)
            pop_pair(P_DE);
        if (mi->imm2 & 2)
            pop_pair(P_DE);
        if (mi->imm2 & 1)
            pop_pair(P_BC);
        if (mi->imm2 & 4)
            pop_pair(P_IY);
        return;
    }
    case M_BR:
        /* To the next block: the branch turned over, to where it would
         * have fallen -- mir_emit makes no jump after it then. */
        if (mi->imm2 == next_blk)
            jump_to_block(mi->imm ^ 0x08, falls_to);
        else
            jump_to_block(mi->imm, mi->imm2);
        return;
    case M_JMP:
        if (mi->imm2 != next_blk)
            jump_to_block(JP_ANY, mi->imm2);
        return;
    case M_RET:
        if (mi->imm2) {
            vpush_const(insns[mi->ssa].in[0].attr.val, mi->type);
        } else if (mi->a >= 0) {
            if (mi->imm) {
                gen_return_hl();
                return;
            }
            vpush(VAL_REG, mi->type, R_HL);
        }
        call(&insns[mi->ssa]);
        return;
    }
    fail = "internal: a machine instruction not made";
}

/* The order the blocks are made in: the SSA form's, each followed by the
 * edge block it falls into and then those its branch jumps to -- each
 * next to where it is reached from, so that what its copies read is not
 * held to the end of the function. */

static void lay_out(void)
{
    int blk, e, k;

    layout = realloc(layout, ((size_t) nmb + 1) * sizeof *layout);
    if (!layout)
        acc_error("out of memory for the machine IR");
    nlayout = 0;
    for (blk = 0; blk != nblocks; blk++) {
        int m = ssa_mb[blk];

        if (blk && rpo_num[blk] < 0)
            continue;
        layout[nlayout++] = m;
        if (edge_after[m] >= 0)
            layout[nlayout++] = edge_after[m];
        for (e = taken_head[m]; e >= 0; e = taken_next[e])
            layout[nlayout++] = e;
    }
    (void) k;
}

/* Where each block falls when it ends without a jump: its last
 * successor. */
static int falls_into(int blk)
{
    MBlock *b = &mb[blk];

    if (!b->nsucc)
        return -1;
    if (b->n && (b->ins[b->n - 1].op == M_JMP || b->ins[b->n - 1].op == M_RET))
        return -1;

    return b->succ[b->nsucc - 1];
}

/* ------------------------------------------------------------------ */
/* the way in                                                          */

int ssa_mir_want, ssa_made_mir;

int mir_on(void)
{
    return ssa_mir_want;
}

static int nparts_made;

int mir_parts(void)
{
    return nparts_made;
}

const char *mir_reason(void)
{
    return mir_why ? mir_why : "?";
}

/* Bounds on the work: rounds of spilling, and the size of function taken
 * at all. */
#define MAX_ROUNDS 16
#define MAX_VREGS  20000

/* The function's machine IR, selected and allocated, before anything is
 * emitted: 0 where it is declined, and why in `fail`'s stead. */
int mir_build(void)
{
    int round, n, blk, at;
    const char *bad;

    if (!mir_ok())
        return 0;
    find_loops();
    nspills = 0;
    sel_fail = NULL;
    setup();
    known_bits();
    select_all();
    if (sel_fail) {
        mir_why = sel_fail;
        return 0;
    }
    if (nvr > MAX_VREGS) {
        mir_why = "a function too big to take";
        return 0;
    }
    link_blocks();
    if (!place_phis())
        return 0;
    dead_code();
    lay_out();
    splitting = ssa_mir_want == 2;
    nparts_made = 0;
    if (splitting) {
        intervals();
        n = nvr;
        if (split_scan() < 0) {
            mir_why = "registers the allocator could not find";
            return 0;
        }
        clob_free();
        bad = verify();
        if (bad) {
            mir_why = bad;
            return 0;
        }
        nparts_made = nvr - n;
        split_resolve();
        if (resolve_bad) {
            mir_why = resolve_bad;
            return 0;
        }
        intervals();
        pcopy_busy();
        npruned = 0;
        (void) check_joined(1);
        if (npruned) {
            clob_free();
            intervals();
            pcopy_busy();
        }
        bad = check_joined(0);
        clob_free();
        if (bad)
            dump_mir("rejected");
        if (bad) {
            mir_why = bad;
            return 0;
        }
    } else {
        for (round = 0; ; round++) {
            intervals();
            n = linear_scan();
            if (n == 0)
                break;
            if (n < 0 || round == MAX_ROUNDS) {
                mir_why = "registers the allocator could not find";
                return 0;
            }
            spill_all();
        }
    }
    {
        int bytes = 0, v;

        for (v = 0; v != nspills; v++)
            bytes += spill_size[v] == 1 ? 1 : ACC_INT_SIZE;
        if (bytes && !gen_local_fits(bytes)) {
            mir_why = "more spills than the frame can reach";
            return 0;
        }
    }
    dump_mir("allocated");
    bad = splitting ? NULL : verify();
    if (bad) {
        mir_why = bad;
        return 0;
    }
    pcopy_busy();
    for (blk = 0; blk != nmb; blk++)
        for (at = 0; at != mb[blk].n; at++)
            if (mb[blk].ins[at].op == M_PCOPY
                && !emit_pcopy(&pc[mb[blk].ins[at].imm],
                               (unsigned) mb[blk].ins[at].imm2, 0)) {
                mir_why = "a cycle of byte copies with no byte free";
                return 0;
            }

    return 1;
}

static void select_all(void)
{
    int blk, at;

    for (blk = 0; blk != nblocks && !sel_fail; blk++) {
        int end = blk + 1 < nblocks ? blocks[blk + 1].first : ninsns;

        if (blk && rpo_num[blk] < 0)
            continue;
        cur = ssa_mb[blk];
        skip_to = -1;
        for (at = blocks[blk].first; at != end && !sel_fail; at++) {
            if (at == 0 || at <= skip_to)
                continue;
            sel_at = at;
            sel_insn(&insns[at], at);
        }
    }
}

/* Whether the code made reads or writes the frame -- a parameter's slot,
 * a spill's, a local's address -- or IX is free and there need be none. */
static int uses_frame(void)
{
    int blk, at;

    for (blk = 0; blk != nmb; blk++)
        for (at = 0; at != mb[blk].n; at++)
            switch (mb[blk].ins[at].op) {
            case M_LDF: case M_STF: case M_STFI: case M_LEAF:
                return 1;
            }

    return 0;
}

void mir_emit(void)
{
    int k, at, *block_start;

    spill_off = realloc(spill_off, ((size_t) nspills + 1) * sizeof *spill_off);
    mb_addr = realloc(mb_addr, ((size_t) nmb + 1) * sizeof *mb_addr);
    pend_head = realloc(pend_head, ((size_t) nmb + 1) * sizeof *pend_head);
    block_start = malloc(((size_t) nblocks + 1) * sizeof *block_start);
    if (!spill_off || !mb_addr || !pend_head || !block_start)
        acc_error("out of memory for the machine IR");
    for (k = 0; k != nmb; k++)
        mb_addr[k] = pend_head[k] = -1;
    for (k = 0; k != nblocks; k++)
        block_start[k] = -1;
    npend = 0;

    blocks_from = out_here();
    call(&insns[0]);
    gen_local_settle();
    for (k = 0; k != nspills; k++)
        spill_off[k] = gen_local(spill_size[k] == 1 ? 1 : ACC_INT_SIZE);

    for (k = 0; k != nlayout && !fail; k++) {
        int blk = layout[k], next = k + 1 < nlayout ? layout[k + 1] : -1, j, f;
        MBlock *b = &mb[blk];

        for (j = pend_head[blk]; j >= 0; j = pend_next[j])
            gen_label(pend_hole[j]);
        pend_head[blk] = -1;
        mb_addr[blk] = gen_here();
        flags_of_a = 0;
        if (b->ssa_block >= 0)
            block_start[b->ssa_block] = mb_addr[blk];
        f = falls_into(blk);
        for (at = 0; at != b->n && !fail; at++) {
            if (b->ins[at].op == M_PCOPY) {
                (void) emit_pcopy(&pc[b->ins[at].imm], (unsigned) b->ins[at].imm2, 1);
                flags_of_a = 0;
            } else if (at + 1 < b->n && byte_via_a(&b->ins[at], &b->ins[at + 1])) {
                flags_of_a = 0;
                at++;
            } else {
                make_mi(&b->ins[at], next, f);
            }
        }
        /* Falling where the next block is not: a jump there -- but for a
         * branch to the next block, which went there turned over. */
        if (f >= 0 && f != next
            && !(b->n && b->ins[b->n - 1].op == M_BR && b->ins[b->n - 1].imm2 == next))
            jump_to_block(JP_ANY, f);
        if (getenv("OPTACC_MIR_DUMP")) {
            int byte;

            fprintf(stderr, "emitted m%d at %x..%x:", blk,
                    mb_addr[blk] - blocks_from, out_here() - blocks_from);
            for (byte = mb_addr[blk]; byte != out_here(); byte++)
                fprintf(stderr, " %02x", out_img[byte - out_base]);
            fprintf(stderr, "\n");
        }
    }
    frame_unused = !uses_frame();
    frame_sp_kept = 1;                  /* every push popped before a return */
    if (!fail)
        costs(block_start, out_here());
    free(block_start);
}

#endif
