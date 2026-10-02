/*
 * opt-acc's machine level: a function's code, once it is made, read back
 * as instructions -- what each reads and writes, register by register and
 * byte by byte -- and what does nothing useful taken out.
 *
 * Whichever backend made the function, the first pass, the hybrid path or
 * the leaf backend, its bytes are the same kind of thing, so this is one
 * place for what none of them sees: a value widened and never read wider,
 * a register loaded and loaded again before it is read, a move through the
 * stack to a register nothing reads, a jump to the next instruction.
 *
 * Read by following the code from the function's start, as it runs --
 * through each jump and past each call -- so the bytes a string or a block
 * static is, which the code jumps over, are never taken for instructions.
 * A function with an instruction not known here, or a jump that lands
 * other than where an instruction starts, is left as it is.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "gen_int.h"
#include "out_int.h"

/* The registers, a bit each: the 24-bit pairs as their three bytes, U the
 * top one, which only an ADL instruction reaches. */
enum {
    RA, RF, RB, RC, RD, RE, RH, RL, RBU, RDU, RHU,
    RIXL, RIXH, RIXU, RIYL, RIYH, RIYU, RSP, MNREGS
};

typedef unsigned Regs;

#define BIT(r)  (1u << (r))
#define M_BC    (BIT(RB) | BIT(RC) | BIT(RBU))
#define M_DE    (BIT(RD) | BIT(RE) | BIT(RDU))
#define M_HL    (BIT(RH) | BIT(RL) | BIT(RHU))
#define M_IX    (BIT(RIXL) | BIT(RIXH) | BIT(RIXU))
#define M_IY    (BIT(RIYL) | BIT(RIYH) | BIT(RIYU))
#define M_SP    BIT(RSP)
#define M_F     BIT(RF)
#define M_A     BIT(RA)
#define M_ALL   ((1u << MNREGS) - 1)
/* What a call may leave changed: IX, IY and SP come back as they went, which
 * a register local in IY kept across calls counts on. */
#define M_CLOBBER (M_A | M_F | M_BC | M_DE | M_HL)

/* What an instruction is, for the flow of control. */
enum {
    K_PLAIN,            /* on to the next */
    K_JUMP,             /* jp or jr, always */
    K_BRANCH,           /* jp or jr on a condition, or djnz */
    K_CALL,             /* call, or a conditional one, or rst */
    K_RET,              /* ret */
    K_RETCC,            /* ret on a condition */
    K_PUSH,             /* push rr: `pair` the register */
    K_POP               /* pop rr */
};

/* What it may do besides its registers: never taken out for being dead. */
#define E_STORE   1     /* writes memory, or ports */
#define E_STACK   2     /* moves sp: push, pop, ex (sp) */
#define E_LOAD    4     /* reads memory other than the frame's */

typedef struct {
    int           at;           /* where it is in the image */
    unsigned char len, kind, effects, gone;
    Regs          use, def;
    Regs          pair;         /* K_PUSH and K_POP: the register */
    int           target;       /* a jump's, or -1 */
    int           next;         /* the instruction after it, by index, or -1 */
    int           to;           /* the target's, by index, or -1 */
    unsigned char labelled;     /* something jumps here */
    unsigned char slot;         /* an operand the link fills in */
    unsigned char runtime;      /* a call into the runtime, by registers */
    unsigned char linked;       /* an operand the link fills in: no constant */
    unsigned char absorbed;     /* its bytes now another's: gone, not cut */
    Regs          live_out;
} MInsn;

static MInsn *ins;
static int    nins, ins_cap;
static int    fn_from, fn_to;   /* the function's bytes */

/* Which instruction starts at each byte of the function, or -1; -2 for a
 * byte inside one. */
static int *at_byte;
static int  at_cap;

static const unsigned char *img(int at)
{
    return out_img + (at - out_base);
}

/* The registers an 8-bit operand names: 0-7 as B C D E H L (HL) A, with
 * an index prefix making H and L its halves. (HL) is memory: none. */
static Regs reg8(int r, int prefix)
{
    switch (r) {
    case 0: return BIT(RB);
    case 1: return BIT(RC);
    case 2: return BIT(RD);
    case 3: return BIT(RE);
    case 4: return prefix == 0xdd ? BIT(RIXH) : prefix == 0xfd ? BIT(RIYH) : BIT(RH);
    case 5: return prefix == 0xdd ? BIT(RIXL) : prefix == 0xfd ? BIT(RIYL) : BIT(RL);
    case 7: return BIT(RA);
    }

    return 0;
}

/* The 24-bit pair an opcode's bits 4-5 name: BC DE HL SP, HL being the
 * index register under a prefix. */
static Regs reg16(int p, int prefix)
{
    switch (p) {
    case 0: return M_BC;
    case 1: return M_DE;
    case 2: return prefix == 0xdd ? M_IX : prefix == 0xfd ? M_IY : M_HL;
    }

    return M_SP;
}

static Regs index_reg(int prefix)
{
    return prefix == 0xdd ? M_IX : M_IY;
}

/* One instruction at `at`, decoded into *m: its length, and 0 for one not
 * known here. */
static int decode(int at, MInsn *m)
{
    const unsigned char *p = img(at);
    int prefix = 0, op, x, y, z, q, pp;

    memset(m, 0, sizeof *m);
    m->at = at;
    m->target = -1;
    m->to = m->next = -1;
    if (p[0] == 0xdd || p[0] == 0xfd) {
        prefix = p[0];
        p++;
    }
    op = p[0];
    x = op >> 6;
    y = (op >> 3) & 7;
    z = op & 7;
    q = y & 1;
    pp = y >> 1;

    if (op == 0xcb) {
        /* rotates and bits: CB op, or DD CB d op on (ix+d) */
        int sub = prefix ? p[2] : p[1], sx = sub >> 6, sz = sub & 7;
        Regs r = prefix ? 0 : reg8(sz, 0);

        m->len = prefix ? 4 : 2;
        if (prefix) {
            m->use |= index_reg(prefix);
            if (sz != 6)
                return 0;           /* the undocumented copies */
        } else if (sz == 6) {
            m->use |= M_HL;
        }
        if (sz == 6) {
            if (sx != 1)
                m->effects |= E_STORE;
            if (!prefix || prefix == 0xfd)
                m->effects |= E_LOAD;
        }
        switch (sx) {
        case 0:                     /* rlc rrc rl rr sla sra - srl */
            if (((sub >> 3) & 7) == 6)
                return 0;
            m->use |= r | M_F;
            m->def |= r | M_F;
            break;
        case 1:                     /* bit: Z, carry kept */
            m->use |= r | M_F;
            m->def |= M_F;
            break;
        default:                    /* res, set */
            m->use |= r;
            m->def |= r;
            break;
        }

        return m->len;
    }

    if (op == 0xed) {
        int e = p[1];

        if (prefix)
            return 0;
        m->len = 2;
        switch (e) {
        case 0x02: case 0x03: case 0x12: case 0x13: case 0x22: case 0x23:
        case 0x32: case 0x33: case 0x54: case 0x55: {
            /* lea rr, ix/iy+d */
            static const struct { unsigned char op; Regs to, from; } lea[] = {
                { 0x02, M_BC, M_IX }, { 0x03, M_BC, M_IY },
                { 0x12, M_DE, M_IX }, { 0x13, M_DE, M_IY },
                { 0x22, M_HL, M_IX }, { 0x23, M_HL, M_IY },
                { 0x32, M_IX, M_IX }, { 0x33, M_IY, M_IY },
                { 0x54, M_IX, M_IY }, { 0x55, M_IY, M_IX },
            };
            unsigned i;

            for (i = 0; i != sizeof lea / sizeof lea[0]; i++)
                if (lea[i].op == e) {
                    m->use = lea[i].from;
                    m->def = lea[i].to;
                }
            m->len = 3;

            return 3;
        }
        case 0x65: case 0x66:       /* pea ix/iy+d */
            m->use = (e == 0x65 ? M_IX : M_IY) | M_SP;
            m->def = M_SP;
            m->effects = E_STACK | E_STORE;
            m->len = 3;

            return 3;
        case 0x07: case 0x17: case 0x27: case 0x31: case 0x37: {
            /* ld rr, (hl) */
            m->use = M_HL;
            m->def = e == 0x07 ? M_BC : e == 0x17 ? M_DE : e == 0x27 ? M_HL
                   : e == 0x31 ? M_IY : M_IX;
            m->effects = E_LOAD;

            return 2;
        }
        case 0x0f: case 0x1f: case 0x2f: case 0x3e: case 0x3f:
            /* ld (hl), rr */
            m->use = M_HL | (e == 0x0f ? M_BC : e == 0x1f ? M_DE
                             : e == 0x2f ? M_HL : e == 0x3e ? M_IY : M_IX);
            m->effects = E_STORE;

            return 2;
        case 0x42: case 0x52: case 0x62: case 0x72:     /* sbc hl, rr */
        case 0x4a: case 0x5a: case 0x6a: case 0x7a:     /* adc hl, rr */
            m->use = M_HL | reg16((e >> 4) & 3, 0) | M_F;
            m->def = M_HL | M_F;
            if (e == 0x62)
                m->use = M_F;       /* sbc hl, hl: 0 or -1, by the carry */

            return 2;
        case 0x43: case 0x53: case 0x63: case 0x73:     /* ld (nn), rr */
            m->use = reg16((e >> 4) & 3, 0);
            m->effects = E_STORE;
            m->len = 5;

            return 5;
        case 0x4b: case 0x5b: case 0x6b: case 0x7b:     /* ld rr, (nn) */
            m->def = reg16((e >> 4) & 3, 0);
            m->effects = E_LOAD;
            m->len = 5;

            return 5;
        case 0x44:                                      /* neg */
            m->use = M_A;
            m->def = M_A | M_F;

            return 2;
        case 0x4c: case 0x5c: case 0x6c:                /* mlt rr */
            m->use = m->def = reg16((e >> 4) & 3, 0);

            return 2;
        case 0xa0: case 0xa8: case 0xb0: case 0xb8:     /* ldi ldd ldir lddr */
            m->use = M_HL | M_DE | M_BC | M_F;
            m->def = M_HL | M_DE | M_BC | M_F;
            m->effects = E_STORE | E_LOAD;

            return 2;
        }

        return 0;
    }

    if (prefix) {
        /* What an index prefix makes of the rest: the HL forms on IX or IY,
         * (HL) as (ix+d), and the eZ80's own loads of a pair from (ix+d). */
        Regs ir = index_reg(prefix);

        switch (op) {
        case 0x07: case 0x17: case 0x27: case 0x31: case 0x37:
            m->use = ir;
            m->def = op == 0x07 ? M_BC : op == 0x17 ? M_DE : op == 0x27 ? M_HL
                   : op == 0x31 ? (prefix == 0xdd ? M_IY : M_IX) : ir;
            if (prefix == 0xfd)
                m->effects = E_LOAD;
            m->len = 3;

            return 3;
        case 0x0f: case 0x1f: case 0x2f: case 0x3e: case 0x3f:
            m->use = ir | (op == 0x0f ? M_BC : op == 0x1f ? M_DE : op == 0x2f ? M_HL
                           : op == 0x3e ? (prefix == 0xdd ? M_IY : M_IX) : ir);
            m->effects = E_STORE;
            m->len = 3;

            return 3;
        case 0x09: case 0x19: case 0x29: case 0x39:     /* add ix, rr */
            m->use = ir | reg16(pp, prefix) | M_F;
            m->def = ir | M_F;
            m->len = 2;

            return 2;
        case 0x21:                                      /* ld ix, nn */
            m->def = ir;
            m->len = 5;

            return 5;
        case 0x22:                                      /* ld (nn), ix */
            m->use = ir;
            m->effects = E_STORE;
            m->len = 5;

            return 5;
        case 0x2a:                                      /* ld ix, (nn) */
            m->def = ir;
            m->effects = E_LOAD;
            m->len = 5;

            return 5;
        case 0x23: case 0x2b:                           /* inc ix, dec ix */
            m->use = m->def = ir;
            m->len = 2;

            return 2;
        case 0x34: case 0x35:                           /* inc (ix+d), dec */
            m->use = ir | M_F;
            m->def = M_F;
            m->effects = E_STORE | (prefix == 0xfd ? E_LOAD : 0);
            m->len = 3;

            return 3;
        case 0x36:                                      /* ld (ix+d), n */
            m->use = ir;
            m->effects = E_STORE;
            m->len = 4;

            return 4;
        case 0xe1:                                      /* pop ix */
            m->kind = K_POP;
            m->pair = ir;
            m->use = M_SP;
            m->def = ir | M_SP;
            m->effects = E_STACK;
            m->len = 2;

            return 2;
        case 0xe5:                                      /* push ix */
            m->kind = K_PUSH;
            m->pair = ir;
            m->use = ir | M_SP;
            m->def = M_SP;
            m->effects = E_STACK;
            m->len = 2;

            return 2;
        case 0xe3:                                      /* ex (sp), ix */
            m->use = m->def = ir | M_SP;
            m->effects = E_STACK | E_STORE;
            m->len = 2;

            return 2;
        case 0xf9:                                      /* ld sp, ix */
            m->use = ir;
            m->def = M_SP;
            m->effects = E_STACK;
            m->len = 2;

            return 2;
        case 0x24: case 0x25: case 0x2c: case 0x2d:     /* inc/dec ixh, ixl */
            m->use = reg8(y, prefix) | M_F;
            m->def = reg8(y, prefix) | M_F;
            m->len = 2;

            return 2;
        case 0x26: case 0x2e:                           /* ld ixh/ixl, n */
            m->def = reg8(y, prefix);
            m->len = 3;

            return 3;
        }
        if (x == 1 && op != 0x76) {
            /* ld r, r' with the index's halves; ld r, (ix+d); ld (ix+d), r */
            if (op == 0x40 || op == 0x49 || op == 0x52 || op == 0x5b
                || op == 0x64 || op == 0x6d || op == 0x7f)
                return 0;
            if (z == 6) {
                m->use = ir;
                m->def = reg8(y, 0);
                if (prefix == 0xfd)
                    m->effects = E_LOAD;
                m->len = 3;
            } else if (y == 6) {
                m->use = ir | reg8(z, 0);
                m->effects = E_STORE;
                m->len = 3;
            } else {
                m->use = reg8(z, prefix);
                m->def = reg8(y, prefix);
                m->len = 2;
            }

            return m->len;
        }
        if (x == 2) {
            /* alu a, ixh / ixl / (ix+d) */
            m->use = M_A | (z == 6 ? ir : reg8(z, prefix));
            if (y == 1 || y == 3)
                m->use |= M_F;
            m->def = (y == 7 ? 0 : M_A) | M_F;
            if (z == 6 && prefix == 0xfd)
                m->effects = E_LOAD;
            m->len = z == 6 ? 3 : 2;

            return m->len;
        }
        if (op == 0xe9)
            return 0;                   /* jp (ix): where to, not known */

        return 0;
    }

    /* Unprefixed. */
    if (x == 1) {
        if (op == 0x76 || op == 0x40 || op == 0x49 || op == 0x52 || op == 0x5b)
            return 0;               /* halt, and the eZ80's mode prefixes */
        m->len = 1;
        if (z == 6) {               /* ld r, (hl) */
            m->use = M_HL;
            m->def = reg8(y, 0);
            m->effects = E_LOAD;
        } else if (y == 6) {        /* ld (hl), r */
            m->use = M_HL | reg8(z, 0);
            m->effects = E_STORE;
        } else {
            m->use = reg8(z, 0);
            m->def = reg8(y, 0);
        }

        return 1;
    }
    if (x == 2) {                   /* alu a, r */
        m->len = 1;
        m->use = M_A | (z == 6 ? M_HL : reg8(z, 0));
        if (y == 1 || y == 3)
            m->use |= M_F;          /* adc, sbc */
        m->def = (y == 7 ? 0 : M_A) | M_F;
        if (z == 6)
            m->effects = E_LOAD;
        if (op == 0x9f)
            m->use = M_F;           /* sbc a, a: 0 or -1, by the carry */
        else if (op == 0x97 || op == 0xaf)
            m->use = 0;             /* sub a, a and xor a, a: 0 */

        return 1;
    }
    if (x == 0) {
        switch (z) {
        case 0:
            if (y == 0) {           /* nop */
                m->len = 1;

                return 1;
            }
            if (y == 1)
                return 0;           /* ex af, af' */
            /* djnz, jr, jr cc */
            m->len = 2;
            m->target = at + 2 + (signed char) p[1];
            if (y == 2) {
                m->kind = K_BRANCH;
                m->use = m->def = BIT(RB);
            } else if (y == 3) {
                m->kind = K_JUMP;
            } else {
                m->kind = K_BRANCH;
                m->use = M_F;
            }

            return 2;
        case 1:
            if (q == 0) {           /* ld rr, nn */
                m->def = reg16(pp, 0);
                m->len = 4;

                return 4;
            }
            m->use = M_HL | reg16(pp, 0) | M_F;     /* add hl, rr */
            m->def = M_HL | M_F;
            m->len = 1;

            return 1;
        case 2:
            m->len = 1;
            switch (y) {
            case 0: case 2:         /* ld (bc), a; ld (de), a */
                m->use = M_A | (y ? M_DE : M_BC);
                m->effects = E_STORE;

                return 1;
            case 1: case 3:         /* ld a, (bc); ld a, (de) */
                m->use = y == 1 ? M_BC : M_DE;
                m->def = M_A;
                m->effects = E_LOAD;

                return 1;
            case 4:                 /* ld (nn), hl */
                m->use = M_HL;
                m->effects = E_STORE;
                m->len = 4;

                return 4;
            case 5:                 /* ld hl, (nn) */
                m->def = M_HL;
                m->effects = E_LOAD;
                m->len = 4;

                return 4;
            case 6:                 /* ld (nn), a */
                m->use = M_A;
                m->effects = E_STORE;
                m->len = 4;

                return 4;
            default:                /* ld a, (nn) */
                m->def = M_A;
                m->effects = E_LOAD;
                m->len = 4;

                return 4;
            }
        case 3:                     /* inc rr, dec rr: no flags */
            m->use = m->def = reg16(pp, 0);
            m->len = 1;

            return 1;
        case 4: case 5:             /* inc r, dec r: carry kept */
            m->len = 1;
            if (y == 6) {
                m->use = M_HL | M_F;
                m->def = M_F;
                m->effects = E_STORE | E_LOAD;
            } else {
                m->use = reg8(y, 0) | M_F;
                m->def = reg8(y, 0) | M_F;
            }

            return 1;
        case 6:                     /* ld r, n */
            m->len = 2;
            if (y == 6) {
                m->use = M_HL;
                m->effects = E_STORE;
            } else {
                m->def = reg8(y, 0);
            }

            return 2;
        default:                    /* rlca rrca rla rra daa cpl scf ccf */
            m->len = 1;
            m->use = M_A | M_F;
            m->def = (y == 6 || y == 7 ? 0 : M_A) | M_F;

            return 1;
        }
    }

    /* x == 3 */
    switch (op) {
    case 0xc3:                      /* jp nn */
        m->kind = K_JUMP;
        m->target = (int) get24(p + 1);
        m->len = 4;

        return 4;
    case 0xcd:                      /* call nn */
        m->kind = K_CALL;
        m->use = M_ALL;
        m->def = M_CLOBBER;
        m->effects = E_STORE | E_STACK;
        m->len = 4;

        return 4;
    case 0xc9:                      /* ret: the caller reads no flags */
        m->kind = K_RET;
        m->use = M_ALL & ~M_F;
        m->len = 1;

        return 1;
    case 0xeb:                      /* ex de, hl */
        m->use = m->def = M_DE | M_HL;
        m->len = 1;

        return 1;
    case 0xe3:                      /* ex (sp), hl */
        m->use = m->def = M_HL | M_SP;
        m->effects = E_STACK | E_STORE;
        m->len = 1;

        return 1;
    case 0xf9:                      /* ld sp, hl */
        m->use = M_HL;
        m->def = M_SP;
        m->effects = E_STACK;
        m->len = 1;

        return 1;
    case 0xd3:                      /* out (n), a */
        m->use = M_A;
        m->effects = E_STORE;
        m->len = 2;

        return 2;
    case 0xdb:                      /* in a, (n) */
        m->def = M_A;
        m->effects = E_STORE;
        m->len = 2;

        return 2;
    }
    switch (z) {
    case 0:                         /* ret cc */
        m->kind = K_RETCC;
        m->use = M_ALL;
        m->len = 1;

        return 1;
    case 1:
        if (q)
            return 0;               /* exx, jp (hl), ld sp, hl: done above */
        m->kind = K_POP;            /* pop rr: AF for pp 3 */
        m->pair = pp == 3 ? M_A | M_F : reg16(pp, 0);
        m->use = M_SP;
        m->def = m->pair | M_SP;
        m->effects = E_STACK;
        m->len = 1;

        return 1;
    case 2:                         /* jp cc, nn */
        m->kind = K_BRANCH;
        m->use = M_F;
        m->target = (int) get24(p + 1);
        m->len = 4;

        return 4;
    case 4:                         /* call cc, nn */
        m->kind = K_CALL;
        m->use = M_ALL;
        m->def = M_CLOBBER;
        m->effects = E_STORE | E_STACK;
        m->len = 4;

        return 4;
    case 5:
        if (q)
            return 0;               /* call nn and the prefixes: done above */
        m->kind = K_PUSH;           /* push rr: AF for pp 3 */
        m->pair = pp == 3 ? M_A | M_F : reg16(pp, 0);
        m->use = m->pair | M_SP;
        m->def = M_SP;
        m->effects = E_STACK;
        m->len = 1;

        return 1;
    case 6:                         /* alu a, n */
        m->use = M_A | (y == 1 || y == 3 ? M_F : 0);
        m->def = (y == 7 ? 0 : M_A) | M_F;
        m->len = 2;

        return 2;
    case 7:                         /* rst: a call */
        m->kind = K_CALL;
        m->use = M_ALL;
        m->def = M_CLOBBER;
        m->effects = E_STORE | E_STACK;
        m->len = 1;

        return 1;
    }

    return 0;
}

/* Why the last function was left as it was, or NULL: OPTACC_PEEP_STATS. */
static const char *refused;

static int add_insn(int at)
{
    MInsn m;

    if (at < fn_from || at >= fn_to) {
        refused = "a jump out of the function";
        return -1;
    }
    if (at_byte[at - fn_from] >= 0)
        return at_byte[at - fn_from];
    if (at_byte[at - fn_from] == -2) {
        refused = "a jump into an instruction";
        return -1;
    }
    if (!decode(at, &m) || at + m.len > fn_to) {
        refused = "an instruction not known here";
        if (getenv("OPTACC_PEEP_STATS")) {
            const unsigned char *p = img(at);

            fprintf(stderr, "peep: not known %02x %02x %02x\n", p[0], p[1], p[2]);
        }
        return -1;
    }
    {
        int b;

        for (b = 1; b != m.len; b++) {
            if (at_byte[at - fn_from + b] != -1) {
                refused = "instructions that overlap";
                return -1;
            }
            at_byte[at - fn_from + b] = -2;
        }
    }
    if (nins == ins_cap) {
        ins_cap = ins_cap ? ins_cap * 2 : 256;
        ins = realloc(ins, (size_t) ins_cap * sizeof *ins);
        if (!ins)
            acc_error("out of memory for the machine code");
    }
    ins[nins] = m;
    at_byte[at - fn_from] = nins;

    return nins++;
}

/* The function's instructions, read from its start along every way the
 * code can go: 0 if some of it could not be. */
static int read_function(void)
{
    int *work = NULL, nwork = 0, cap = 0, i;

    nins = 0;
    if (fn_to - fn_from + 1 > at_cap) {
        at_cap = (fn_to - fn_from + 1) * 2;
        at_byte = realloc(at_byte, (size_t) at_cap * sizeof *at_byte);
        if (!at_byte)
            acc_error("out of memory for the machine code");
    }
    for (i = 0; i != fn_to - fn_from; i++)
        at_byte[i] = -1;

#define WORK(a) do {                                                     \
        if (nwork == cap) {                                              \
            cap = cap ? cap * 2 : 64;                                    \
            work = realloc(work, (size_t) cap * sizeof *work);           \
            if (!work)                                                   \
                acc_error("out of memory for the machine code");         \
        }                                                                \
        work[nwork++] = (a);                                             \
    } while (0)

    WORK(fn_from);
    while (nwork) {
        int at = work[--nwork];

        while (at < fn_to) {
            int known = at_byte[at - fn_from] >= 0, k;
            MInsn *m;

            k = add_insn(at);
            if (k < 0) {
                free(work);
                return 0;
            }
            if (known)
                break;
            m = &ins[k];
            if (m->target >= 0)
                WORK(m->target);
            if (m->kind == K_JUMP || m->kind == K_RET)
                break;
            at += m->len;
        }
    }
    free(work);
#undef WORK

    /* In order, each knowing the next and its target. */
    {
        int n = 0, at;

        for (at = 0; at != fn_to - fn_from; at++)
            if (at_byte[at] >= 0) {
                ins[at_byte[at]].next = n;      /* its place, for now */
                n++;
            }
    }
    {
        MInsn *sorted = malloc((size_t) (nins ? nins : 1) * sizeof *sorted);

        if (!sorted)
            acc_error("out of memory for the machine code");
        for (i = 0; i != nins; i++)
            sorted[ins[i].next] = ins[i];
        memcpy(ins, sorted, (size_t) nins * sizeof *ins);
        free(sorted);
    }
    for (i = 0; i != nins; i++)
        at_byte[ins[i].at - fn_from] = i;
    for (i = 0; i != nins; i++) {
        MInsn *m = &ins[i];
        int after = m->at + m->len;

        m->next = after < fn_to && at_byte[after - fn_from] >= 0
                  ? at_byte[after - fn_from] : -1;
        if (m->target >= 0) {
            m->to = at_byte[m->target - fn_from];
            ins[m->to].labelled = 1;
        }
    }
    if (nins)
        ins[0].labelled = 1;            /* the function's start */

    return 1;
}

/* The constant pool's uses, each marked on its instruction, which is then
 * never taken out: relax.c holds that the pool's list keeps its length.
 * (A relocation or a fixup in an instruction taken out goes with it.) And
 * an address the function holds of its own code: then that code is reached
 * some way not followed here, and the function is left as it is. */
static int mark_slots(const Mark *from)
{
    int i;
    const int *r;

#define SLOT(a) do {                                                     \
        int a_ = (a);                                                    \
        if (a_ >= fn_from && a_ < fn_to) {                               \
            int k_ = a_ - fn_from;                                       \
            while (k_ > 0 && at_byte[k_] == -2)                          \
                k_--;                                                    \
            if (at_byte[k_] >= 0)                                        \
                ins[at_byte[k_]].slot = 1;                               \
        }                                                                \
    } while (0)

    for (r = out_relocs + 1 + from->reloc; r < out_reloc_put; r++) {
        int v;

        if ((unsigned) *r < (unsigned) out_flushed)
            continue;
        v = (int) get24(out_img + *r);
        if (v < fn_from || v >= fn_to || at_byte[v - fn_from] == -1
            || v == fn_from)
            continue;               /* elsewhere, data, or a call to itself */
        {
            int a = out_base + *r, k = a - fn_from;

            /* A jump's own operand: where it goes is known. */
            if (a > fn_from && a < fn_to) {
                while (k > 0 && at_byte[k] == -2)
                    k--;
                if (at_byte[k] >= 0 && ins[at_byte[k]].target == v)
                    continue;
            }
        }
        {
            refused = "an address of the function's own code";
            return 0;
        }
    }
    for (i = 0; i != npool_sites; i++)
        SLOT(pool_site_at[i]);
#undef SLOT
#define LINKED(a) do {                                                   \
        int k_ = (a) - fn_from;                                          \
        if (k_ > 0 && k_ < fn_to - fn_from) {                            \
            while (k_ > 0 && at_byte[k_] == -2)                          \
                k_--;                                                    \
            if (at_byte[k_] >= 0)                                        \
                ins[at_byte[k_]].linked = 1;                             \
        }                                                                \
    } while (0)

    /* What the link fills in is no constant here: a call or a load waiting
     * on a symbol holds what to add to its address, an address in the bss
     * an offset into it, and one in the image changes as the image is
     * placed. */
    for (r = out_relocs + 1 + from->reloc; r < out_reloc_put; r++)
        LINKED(out_base + *r);
    for (i = from->fixup; i != nfixups; i++)
        LINKED(fixup_at(i)->at);
    for (i = from->rt; i != nrt_fixups; i++)
        LINKED(rt_fixups[i].at);
    for (i = from->bss; i != nbss_fixups; i++)
        LINKED(*bss_fixup(i));
    for (i = 0; i != npool_sites; i++)
        LINKED(pool_site_at[i]);
#undef LINKED

    /* A call into the runtime takes its operands in registers; a call to
     * a C function, on the stack -- it reads none of them, and leaves IX
     * and SP as they were. */
    for (i = from->rt; i != nrt_fixups; i++) {
        int k = rt_fixups[i].at - fn_from;

        if (k <= 0 || k >= fn_to - fn_from)
            continue;
        while (k > 0 && at_byte[k] == -2)
            k--;
        if (at_byte[k] >= 0)
            ins[at_byte[k]].runtime = 1;
    }
    for (i = 0; i != nins; i++)
        if (ins[i].kind == K_CALL && !ins[i].runtime
            && (img(ins[i].at)[0] & 0xc7) != 0xc7)     /* not rst */
            ins[i].use = M_SP | (img(ins[i].at)[0] == 0xcd ? 0 : M_F);

    return 1;
}

/* What each instruction leaves live: the registers some way on from it
 * reads before writing. A partial write -- a flag kept, a byte of a pair
 * -- is a read as well, as decode has it. */
static void liveness(void)
{
    int changed, i;

    for (i = 0; i != nins; i++)
        ins[i].live_out = 0;
    do {
        changed = 0;
        for (i = nins - 1; i >= 0; i--) {
            MInsn *m = &ins[i];
            Regs out = 0, in_next;

            if (m->gone)
                continue;
            if (m->kind != K_JUMP && m->kind != K_RET && m->next >= 0) {
                int n = m->next;

                while (n >= 0 && ins[n].gone)
                    n = ins[n].next;
                if (n >= 0) {
                    in_next = (ins[n].live_out & ~ins[n].def) | ins[n].use;
                    out |= in_next;
                }
            }
            if (m->to >= 0) {
                int n = m->to;

                while (n >= 0 && ins[n].gone)
                    n = ins[n].next;
                if (n >= 0)
                    out |= (ins[n].live_out & ~ins[n].def) | ins[n].use;
            }
            if (out != m->live_out) {
                m->live_out = out;
                changed = 1;
            }
        }
    } while (changed);
}

/* The instruction after `i` that is still there, or -1. */
static int after(int i)
{
    int n = ins[i].next;

    while (n >= 0 && ins[n].gone)
        n = ins[n].next;

    return n;
}

static int npeep_gone, npeep_bytes;

static void take(int i)
{
    ins[i].gone = 1;
    npeep_gone++;
    npeep_bytes += ins[i].len;
}

/* ------------------------------------------------------------------ */
/* what each byte holds                                                */

/* A byte's contents, as far as they are known within a run of code no
 * jump lands in: a value's byte, (value << 2) | byte, or 0 for not known.
 * A value is made where a byte nothing known put there is read, so that
 * whatever it is copied to holds the same; each constant is a value. */
typedef unsigned Holds;

static unsigned nvalues;
static Holds    now[MNREGS];
static Holds    frame[260];             /* (ix+d), d from -128 */
static Holds    stk[32][3];             /* what each push put there */
static int      nstk;                   /* -1: not known */
static struct { int n; unsigned value; } *consts;
static int      nconsts, consts_cap;

static const unsigned char pair_regs[5][3] = {
    { RC, RB, RBU }, { RE, RD, RDU }, { RL, RH, RHU },
    { RIXL, RIXH, RIXU }, { RIYL, RIYH, RIYU },
};

static int pair_index(Regs pair)
{
    return pair == M_BC ? 0 : pair == M_DE ? 1 : pair == M_HL ? 2
         : pair == M_IX ? 3 : pair == M_IY ? 4 : -1;
}

static Holds holds(unsigned value, int byte)
{
    return value << 2 | (unsigned) byte;
}

static unsigned const_value(int n)
{
    int i;

    n &= 0xffffff;
    for (i = 0; i != nconsts; i++)
        if (consts[i].n == n)
            return consts[i].value;
    if (nconsts == consts_cap) {
        consts_cap = consts_cap ? consts_cap * 2 : 32;
        consts = realloc(consts, (size_t) consts_cap * sizeof *consts);
        if (!consts)
            acc_error("out of memory for the machine code");
    }
    consts[nconsts].n = n;
    consts[nconsts].value = ++nvalues;

    return consts[nconsts++].value;
}

static Holds need_reg(int r)
{
    if (!now[r])
        now[r] = holds(++nvalues, 0);

    return now[r];
}

static Holds need_frame(int d)
{
    if (d < -128 || d > 131)
        return 0;
    if (!frame[d + 128])
        frame[d + 128] = holds(++nvalues, 0);

    return frame[d + 128];
}

static void set_frame(int d, Holds h)
{
    if (d >= -128 && d <= 131)
        frame[d + 128] = h;
}

static void forget_frame(void)
{
    memset(frame, 0, sizeof frame);
}

static void forget_all(void)
{
    memset(now, 0, sizeof now);
    forget_frame();
    nstk = 0;
}

/* Whether the bytes `to` already hold `what`, every one of them known. */
static int same3(const unsigned char *to, const Holds *what)
{
    int b;

    for (b = 0; b != 3; b++)
        if (!what[b] || now[to[b]] != what[b])
            return 0;

    return 1;
}

static void set3(const unsigned char *to, const Holds *what)
{
    int b;

    for (b = 0; b != 3; b++)
        now[to[b]] = what[b];
}

/* The 8-bit register an operand names, as a byte's index, or -1 for one
 * that is memory. */
static int byte_reg(int r, int prefix)
{
    static const signed char plain[8] = { RB, RC, RD, RE, RH, RL, -1, RA };

    if (r == 4 && prefix)
        return prefix == 0xdd ? RIXH : RIYH;
    if (r == 5 && prefix)
        return prefix == 0xdd ? RIXL : RIYL;

    return plain[r];
}

/* One instruction run forward over what is known: 1 if it changes nothing
 * that is -- every byte it writes holds that already -- so that it may go. */
static int simulate(const MInsn *m)
{
    const unsigned char *p = img(m->at);
    int prefix = 0, op, r;
    Holds three[3];

    if (p[0] == 0xdd || p[0] == 0xfd)
        prefix = *p++;
    op = p[0];

    switch (m->kind) {
    case K_PUSH: {
        int k = pair_index(m->pair);

        if (nstk < 0 || nstk == 32) {
            nstk = -1;
            return 0;
        }
        if (k < 0) {                    /* af: only A is followed */
            stk[nstk][0] = need_reg(RA);
            stk[nstk][1] = stk[nstk][2] = 0;
        } else {
            for (r = 0; r != 3; r++)
                stk[nstk][r] = need_reg(pair_regs[k][r]);
        }
        nstk++;

        return 0;
    }
    case K_POP: {
        int k = pair_index(m->pair);

        if (k < 0) {
            now[RA] = nstk > 0 ? stk[nstk - 1][0] : 0;
        } else if (nstk > 0) {
            set3(pair_regs[k], stk[nstk - 1]);
        } else {
            for (r = 0; r != 3; r++)
                now[pair_regs[k][r]] = 0;
        }
        nstk = nstk > 0 ? nstk - 1 : -1;
        if (m->pair == M_IX)
            forget_frame();

        return 0;
    }
    case K_CALL: {
        int i;

        for (r = 0; r != MNREGS; r++)
            if (r != RIXL && r != RIXH && r != RIXU && r != RSP)
                now[r] = 0;
        for (i = 0; i < nstk; i++)
            stk[i][0] = stk[i][1] = stk[i][2] = 0;  /* the callee's to write */
        forget_frame();

        return 0;
    }
    }

    if (!prefix) {
        if ((op == 0x01 || op == 0x11 || op == 0x21) && !m->linked) {
            /* ld rr, nn */
            const unsigned char *to = pair_regs[op == 0x01 ? 0 : op == 0x11 ? 1 : 2];
            unsigned v = const_value((int) get24(p + 1));
            int same;

            for (r = 0; r != 3; r++)
                three[r] = holds(v, r);
            same = same3(to, three);
            set3(to, three);

            return same;
        }
        if ((op & 0xc7) == 0x06 && op != 0x36) {              /* ld r, n */
            int to = byte_reg(op >> 3 & 7, 0);
            Holds h = holds(const_value(p[1]), 0);
            int same = now[to] == h;

            now[to] = h;

            return same;
        }
        if (op >= 0x40 && op < 0x80 && (op & 7) != 6 && (op >> 3 & 7) != 6) {
            int to = byte_reg(op >> 3 & 7, 0), from = byte_reg(op & 7, 0);
            Holds h = need_reg(from);
            int same = now[to] == h;

            now[to] = h;

            return same;
        }
        if (op == 0xeb) {                                       /* ex de, hl */
            Holds t[3];

            for (r = 0; r != 3; r++) {
                t[r] = now[pair_regs[1][r]];
                now[pair_regs[1][r]] = now[pair_regs[2][r]];
                now[pair_regs[2][r]] = t[r];
            }

            return 0;
        }
        if (op == 0xed) {
            int e = p[1];
            signed char d = (signed char) p[2];

            switch (e) {
            case 0x02: case 0x03: case 0x12: case 0x13: case 0x22: case 0x23:
            case 0x32: case 0x33: case 0x54: case 0x55: {      /* lea */
                int to = (e == 0x02 || e == 0x03) ? 0 : (e == 0x12 || e == 0x13) ? 1
                       : (e == 0x22 || e == 0x23) ? 2 : (e == 0x32 || e == 0x54) ? 3 : 4;
                int from = (e == 0x02 || e == 0x12 || e == 0x22 || e == 0x32
                            || e == 0x55) ? 3 : 4;
                int same = 0;

                if (d == 0) {
                    for (r = 0; r != 3; r++)
                        three[r] = need_reg(pair_regs[from][r]);
                    same = same3(pair_regs[to], three);
                    set3(pair_regs[to], three);
                } else {
                    for (r = 0; r != 3; r++)
                        now[pair_regs[to][r]] = 0;
                }
                if (to == 3)
                    forget_frame();     /* IX moved: the frame is elsewhere */

                return same;
            }
            }
        }
    } else if (prefix == 0xdd) {
        signed char d = (signed char) p[1];

        if (op == 0x07 || op == 0x17 || op == 0x27 || op == 0x31) {
            /* ld rr, (ix+d) */
            int to = op == 0x07 ? 0 : op == 0x17 ? 1 : op == 0x27 ? 2 : 4;
            int same;

            for (r = 0; r != 3; r++)
                three[r] = need_frame(d + r);
            same = same3(pair_regs[to], three);
            set3(pair_regs[to], three);

            return same;
        }
        if (op == 0x0f || op == 0x1f || op == 0x2f || op == 0x3e) {
            /* ld (ix+d), rr */
            int from = op == 0x0f ? 0 : op == 0x1f ? 1 : op == 0x2f ? 2 : 4;
            int same = 1;

            for (r = 0; r != 3; r++) {
                Holds h = need_reg(pair_regs[from][r]);

                if (d + r < -128 || d + r > 131 || frame[d + r + 128] != h)
                    same = 0;
                set_frame(d + r, h);
            }

            return same;
        }
        if (op >= 0x40 && op < 0x80 && op != 0x76 && (op & 7) == 6) {
            /* ld r, (ix+d) */
            int to = byte_reg(op >> 3 & 7, 0);
            Holds h = need_frame(d);
            int same = now[to] == h;

            now[to] = h;

            return same;
        }
        if (op >= 0x70 && op < 0x78 && op != 0x76) {
            /* ld (ix+d), r */
            Holds h = need_reg(byte_reg(op & 7, 0));
            int same = frame[d + 128] == h;   /* d is a byte: in reach */

            set_frame(d, h);

            return same;
        }
        if (op == 0x36) {                                       /* ld (ix+d), n */
            Holds h = holds(const_value(p[2]), 0);
            int same = frame[d + 128] == h;   /* d is a byte: in reach */

            set_frame(d, h);

            return same;
        }
    }

    /* Anything else: what it writes is not known, and a store anywhere
     * but the frame may be into it -- a local whose address is taken. */
    for (r = 0; r != MNREGS; r++)
        if (m->def & BIT(r))
            now[r] = 0;
    if ((m->effects & E_STORE) || (m->def & M_IX))
        forget_frame();
    if ((m->effects & E_STACK) || (m->def & M_SP))
        nstk = -1;

    return 0;
}

/* The instructions that change nothing known taken out; and push rr /
 * pop rr' where rr' holds what rr does already. */
static int values(void)
{
    int i, any = 0;

    nconsts = 0;
    forget_all();
    for (i = 0; i != nins; i++) {
        MInsn *m = &ins[i];
        int n;

        if (m->gone)
            continue;
        if (m->labelled)
            forget_all();
        if (m->kind == K_PUSH && !m->slot && !m->labelled && (n = after(i)) >= 0
            && n == m->next && ins[n].kind == K_POP && !ins[n].labelled
            && pair_index(m->pair) >= 0 && pair_index(ins[n].pair) >= 0) {
            const unsigned char *from = pair_regs[pair_index(m->pair)];
            const unsigned char *to = pair_regs[pair_index(ins[n].pair)];
            Holds h[3];
            int b;

            for (b = 0; b != 3; b++)
                h[b] = need_reg(from[b]);
            if (same3(to, h)) {
                take(i);
                take(n);
                any = 1;
                continue;
            }
        }
        if (simulate(m) && !m->labelled && !m->slot && m->kind == K_PLAIN
            && !(m->def & M_F & m->live_out) && !(m->effects & ~E_STORE)) {
            take(i);
            any = 1;
        }
    }

    return any;
}

/* Instruction `i` made another of the same length, `op` its one byte. */
static void rewrite(int i, unsigned char op)
{
    MInsn keep = ins[i];

    out_img[keep.at - out_base] = op;
    decode(keep.at, &ins[i]);
    ins[i].labelled = keep.labelled;
    ins[i].slot = keep.slot;
    ins[i].linked = keep.linked;
    ins[i].runtime = keep.runtime;
    ins[i].next = keep.next;
    ins[i].to = keep.to;
    ins[i].live_out = keep.live_out;
}

/* The code for ld r, r': registers as an operand names them. */
static unsigned char ld_rr(int to, int from)
{
    return (unsigned char) (0x40 | to << 3 | from);
}

/* A register's number as an operand names it: B C D E H L - A. */
static int operand_of(Regs r)
{
    return r == BIT(RB) ? 0 : r == BIT(RC) ? 1 : r == BIT(RD) ? 2
         : r == BIT(RE) ? 3 : r == BIT(RH) ? 4 : r == BIT(RL) ? 5 : 7;
}

/* A value pushed to wait while the code after it runs, and popped where it
 * is wanted: kept in a register that code leaves alone instead. Nothing
 * between may move SP, call, jump or be jumped to.
 *   push rr / ... / pop rr, nothing between writing rr: both go.
 *   push hl / ... / pop hl, nothing between touching DE, HL written there
 *     before it is read, and DE not read after: ex de, hl at both ends --
 *     the same of DE, with HL.
 *   push af / ... / pop rr, the byte wanted in rr's high byte, its low and
 *     U bytes not read after (the pop leaves them the flags and junk): ld
 *     h, a and no pop, where nothing between touches H; or, where something
 *     does, ld r, a and ld h, r, r a byte nothing between touches and that
 *     is read by nothing after the push.
 *   push hl / ... / pop de: ex de, hl, and no pop, where nothing between
 *     touches DE, and no byte of HL is live past the push -- read between
 *     before it is written, or kept to be read after the pop; push de /
 *     ... / pop hl the same way round. */
static int park(int i)
{
    const MInsn *m = &ins[i], *pop;
    Regs touched = 0, wrote = 0, read_first = 0;
    int n = after(i);

    for (; n >= 0; n = after(n)) {
        const MInsn *x = &ins[n];

        if (x->labelled)
            return 0;
        if (x->kind == K_POP)
            break;
        if (x->kind != K_PLAIN || (x->effects & E_STACK)
            || ((x->use | x->def) & M_SP))
            return 0;
        touched |= x->use | x->def;
        read_first |= x->use & ~wrote;
        wrote |= x->def;
    }
    if (n < 0 || ins[n].slot)
        return 0;
    pop = &ins[n];

    if (m->pair == pop->pair && pair_index(m->pair) >= 0) {
        Regs other = m->pair == M_HL ? M_DE : m->pair == M_DE ? M_HL : 0;

        if (!(wrote & m->pair)) {
            take(i);
            take(n);

            return 1;
        }
        if (other && !(touched & other) && !(read_first & m->pair)
            && !(pop->live_out & other)) {
            rewrite(i, 0xeb);
            rewrite(n, 0xeb);

            return 1;
        }

        return 0;
    }
    if (m->pair == (M_A | M_F)) {
        Regs high = pop->pair == M_DE ? BIT(RD) : pop->pair == M_HL ? BIT(RH)
                  : pop->pair == M_BC ? BIT(RB) : 0;
        static const unsigned char spare[] = { RB, RC, RD, RE, RH, RL };
        unsigned k;

        if (!high || (pop->live_out & pop->pair & ~high))
            return 0;
        if (!(touched & high)) {
            rewrite(i, ld_rr(operand_of(high), 7));
            take(n);

            return 1;
        }
        for (k = 0; k != sizeof spare; k++) {
            Regs r = BIT(spare[k]);

            if (r == high || (touched & r) || (m->live_out & r))
                continue;
            rewrite(i, ld_rr(operand_of(r), 7));
            rewrite(n, ld_rr(operand_of(high), operand_of(r)));

            return 1;
        }

        return 0;
    }
    if ((m->pair == M_HL && pop->pair == M_DE)
        || (m->pair == M_DE && pop->pair == M_HL)) {
        if ((touched & pop->pair) || (m->live_out & m->pair))
            return 0;
        rewrite(i, 0xeb);
        take(n);

        return 1;
    }

    return 0;
}

/* push hl / a load of HL / ex de, hl / pop hl: the value loaded into DE,
 * HL left as it was -- the same load, made into DE. ld hl, (nn) has no
 * DE form one byte long: ld de, (nn) is ED 5B nn, written over the push
 * and the load's opcode, so that nn stays where the link wants it. */
static int load_to_de(int i)
{
    int load = after(i), ex, pop;
    unsigned char *p;

    if (load < 0 || ins[load].labelled || (ex = after(load)) < 0
        || ins[ex].labelled || img(ins[ex].at)[0] != 0xeb || ins[ex].len != 1
        || (pop = after(ex)) < 0 || ins[pop].labelled
        || ins[pop].kind != K_POP || ins[pop].pair != M_HL
        || load != ins[i].next || ex != ins[load].next || pop != ins[ex].next)
        return 0;
    p = out_img + (ins[load].at - out_base);
    if (p[0] == 0x21) {
        p[0] = 0x11;                            /* ld de, nn */
    } else if ((p[0] == 0xdd || p[0] == 0xfd) && p[1] == 0x27) {
        p[1] = 0x17;                            /* ld de, (ix+d) */
    } else if (p[0] == 0xed && (p[1] == 0x22 || p[1] == 0x23)) {
        p[1] = (unsigned char) (p[1] - 0x10);   /* lea de, ix+d */
    } else if (p[0] == 0x2a && !ins[i].labelled) {
        MInsn keep = ins[i];

        out_img[keep.at - out_base] = 0xed;     /* ld de, (nn) */
        p[0] = 0x5b;
        decode(keep.at, &ins[i]);
        ins[i].labelled = keep.labelled;
        ins[i].next = ins[load].next;
        ins[i].slot = ins[load].slot;
        ins[i].linked = ins[load].linked;
        ins[load].gone = ins[load].absorbed = 1;
        take(ex);
        take(pop);
        npeep_bytes -= 1;                       /* one byte longer than it was */

        return 1;
    } else {
        return 0;
    }
    {
        MInsn keep = ins[load];

        decode(keep.at, &ins[load]);
        ins[load].labelled = keep.labelled;
        ins[load].slot = keep.slot;
        ins[load].linked = keep.linked;
        ins[load].next = keep.next;
    }
    take(i);
    take(ex);
    take(pop);

    return 1;
}

/* One pass of the rules over the function: whether anything went. */
static int rules(void)
{
    int i, any = 0;

    liveness();
    if (values())
        return 1;
    for (i = 0; i != nins; i++) {
        MInsn *m = &ins[i];
        int n;

        /* Never what something jumps to: relax.c holds a target that is
         * gone to be nowhere. */
        if (m->gone || m->slot || m->labelled)
            continue;

        /* A jump to where the code goes next anyway. */
        if ((m->kind == K_JUMP || (m->kind == K_BRANCH && !(m->use & BIT(RB))))
            && m->to >= 0 && (n = after(i)) >= 0 && n == m->to) {
            take(i);
            any = 1;
            continue;
        }

        /* What it makes, nothing reads, and it does nothing else. */
        if (m->kind == K_PLAIN && !m->effects && m->def
            && !(m->def & m->live_out)) {
            take(i);
            any = 1;
            continue;
        }

        /* push rr / pop rr': a move, with nothing to show for it -- the
         * register it makes not read, or the same register. */
        if (m->kind == K_PUSH && (n = after(i)) >= 0 && n == m->next
            && ins[n].kind == K_POP && !ins[n].labelled && !ins[n].slot
            && (ins[n].pair == m->pair || !(ins[n].pair & ins[n].live_out))) {
            take(i);
            take(n);
            any = 1;
            continue;
        }

        /* These two make instructions other ones: what is live where is
         * then not what liveness said, and the pass ends here. Taking an
         * instruction out only makes less live, which the rules above may
         * go on from. */
        if (m->kind == K_PUSH && m->pair == M_HL && load_to_de(i))
            return 1;
        if (m->kind == K_PUSH && park(i))
            return 1;
    }

    return any;
}

/* The instructions taken out, out of the image: cut, with every list of
 * places brought along by relax.c, and each relative jump written again
 * for where it and its target are now. */
static void cut_gone(const Mark *from)
{
    Cut *cuts = NULL;
    int ncuts = 0, i;

    for (i = 0; i != nins; i++)
        if (ins[i].gone && !ins[i].absorbed)
            ncuts++;
    if (!ncuts)
        return;
    cuts = malloc((size_t) ncuts * sizeof *cuts);
    if (!cuts)
        acc_error("out of memory for the machine code");
    ncuts = 0;
    for (i = 0; i != nins; i++)
        if (ins[i].gone && !ins[i].absorbed) {
            if (ncuts && cuts[ncuts - 1].at + cuts[ncuts - 1].len == ins[i].at)
                cuts[ncuts - 1].len += ins[i].len;
            else {
                cuts[ncuts].at = ins[i].at;
                cuts[ncuts].len = ins[i].len;
                ncuts++;
            }
        }
    relax_cut_code(cuts, ncuts, from, fn_from, fn_to);

    /* The jumps that say where they go as a distance: the targets and the
     * jumps both moved, and the distance only shrinks. */
    for (i = 0; i != nins; i++) {
        const MInsn *m = &ins[i];
        int now, to;
        unsigned char *p;

        if (m->gone || m->len != 2 || m->target < 0)
            continue;
        now = out_cut_moved(m->at);
        to = out_cut_moved(m->target);
        if (now < 0 || to < 0)
            acc_error("internal: a relative jump lost its place");
        p = out_img + (now - out_base);
        p[1] = (unsigned char) (to - (now + 2));
    }
    free(cuts);
}

/* The function from `start` to `end` read and its rules run, nothing cut
 * yet: 0 where it is left as it is. test/test_peep.c asks this, and then
 * peep_gone, of code it writes itself. */
int peep_analyse(const Mark *from, int start, int end)
{
    fn_from = start;
    fn_to = end;
    refused = NULL;
    npeep_gone = npeep_bytes = 0;
    if (fn_from - out_base < out_flushed) {
        refused = "the function is in the file already";
        return 0;
    }
    if (!read_function() || !mark_slots(from)) {
        nins = 0;
        return 0;
    }
    while (rules())
        ;

    return 1;
}

/* Whether the instruction at `at` was taken out. */
int peep_gone(int at)
{
    int k = at - fn_from;

    return k >= 0 && k < fn_to - fn_from && at_byte[k] >= 0
           && ins[at_byte[k]].gone;
}

/* The function from `start` to here, its bookkeeping from `from` on. */
void peep_function(const Mark *from, int start)
{
    if (!getenv("OPTACC_PEEP"))
        return;
    if (peep_analyse(from, start, out_here()))
        cut_gone(from);
    if (getenv("OPTACC_PEEP_STATS"))
        fprintf(stderr, "peep %d insns, %d gone, %d bytes%s%s\n", nins,
                npeep_gone, npeep_bytes, refused ? ": " : "",
                refused ? refused : "");
}
