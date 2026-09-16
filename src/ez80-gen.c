/*
 *  eZ80 code generator for acc
 *
 *  Copyright (c) 2026 Igor Cananea
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef TARGET_DEFS_ONLY

/* The three 24-bit general registers in ADL mode. IX is the frame pointer --
 * agondev's prologue is `call __frameset0` and its epilogue `pop ix` -- and IY
 * is left out on purpose: the optimization guide's measurement is that one
 * index register is the budget inside a loop, and handing the allocator a
 * second one invites spills that cost more than the register saves. */
#define NB_REGS             3
#define NB_ASM_REGS         3

/* Register classes, sorted general to precise; gv2() depends on that order. */
#define RC_INT     0x0001
#define RC_FLOAT   0x0002
#define RC_HL      0x0004
#define RC_DE      0x0008
#define RC_BC      0x0010

#define RC_IRET    RC_HL   /* function return: integer register */
#define RC_IRE2    RC_DE   /* function return: second integer register */
#define RC_FRET    RC_HL   /* function return: float register */

enum {
    TREG_HL = 0,
    TREG_DE,
    TREG_BC,
    TREG_MEM = 0x20
};

#define REG_IRET TREG_HL   /* single word int return register */
#define REG_IRE2 TREG_DE   /* second word return register */
#define REG_FRET TREG_HL   /* float return register */

/* Measured from agondev's own output: a value is returned low bytes first in
 * HL, then DE, then BC.
 *
 *   int, pointer  (3)  HL
 *   long, float   (4)  HL = bits 0-23, E = bits 24-31
 *   long long     (8)  HL = 0-2, DE = 3-5, BC = 6-7
 *   long double   (8)  same as long long
 *
 * The 8-byte case wants three registers and an SValue has room for two, r and
 * r2. That is not worked around here: 8-byte values stay in memory, which is
 * what the eZ80 would mostly do with them anyway. */

/* Arguments are pushed, so they are evaluated right to left. */
#define INVERT_FUNC_PARAMS

/* Measured: a struct returned by value is written through a hidden pointer
 * passed as the first argument, and that pointer comes back in HL. */
#define FUNC_STRUCT_PARAM_AS_PTR

/* pointer size, in bytes */
#define PTR_SIZE 3

/* int is 24 bits on this target and long is 32, which is why this macro has
 * to exist at all: tinycc assumes sizeof(int) == 4 wherever it does not go
 * through type_size(). LONG_SIZE falls out of tcc.h correctly for PTR_SIZE 3,
 * so it is not defined here. */
#define INT_SIZE 3

/* long double is a true IEEE binary64 on this target, reached through
 * agondev's __d* helpers. `double` is not: it is the same four bytes as
 * float, using the __f* helpers. Matching that is what lets acc's output
 * link against libagon.a. */
#define DOUBLE_SIZE   4
#define LDOUBLE_SIZE  8
#define LDOUBLE_ALIGN 1

/* The eZ80 has no alignment requirement -- every load and store works at any
 * address -- so nothing is aligned unless __attribute__((aligned)) asks. */
#define TARGET_ALIGN_1
#define MAX_ALIGN     8

/* Return values need extending at the caller for agondev's clang. */
#define PROMOTE_RET

/******************************************************/
#else /* ! TARGET_DEFS_ONLY */
/******************************************************/

/* tcc_error and friends reach the compiler state through a local s1 unless a
 * translation unit asks for the global one. A backend has no s1 to hand. */
#define USING_GLOBALS
#include "tcc.h"

ST_DATA const char * const target_machine_defs =
    "__ez80__\0"
    "__AGON__\0"
    ;

ST_DATA const int reg_classes[NB_REGS] = {
    /* HL */ RC_INT | RC_FLOAT | RC_HL,
    /* DE */ RC_INT | RC_FLOAT | RC_DE,
    /* BC */ RC_INT | RC_FLOAT | RC_BC,
};

/* Z80 encodes a register pair as a two-bit field: BC=0, DE=1, HL=2. acc
 * numbers them HL, DE, BC so that HL, the one almost everything returns in,
 * is register 0. This maps between the two. */
static const unsigned char pp_of[NB_REGS] = { 2, 1, 0 };

/* Where a function's frame is, and how big. */
static int func_sub_sp_offset;   /* patch site for the frame size */
static int func_ret_sub;         /* bytes of arguments the callee leaves */
static int func_frame_size;      /* locals, in bytes */
static int func_uses_frame;      /* whether __frameset was emitted */

/* ------------------------------------------------------------------ */
/* emitting bytes                                                      */

static void g(int c)
{
    int ind1 = ind + 1;

    if (nocode_wanted)
        return;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind] = c;
    ind = ind1;
}

ST_FUNC void o(unsigned int c)
{
    while (c) {
        g(c);
        c = c >> 8;
    }
}

static void gen_le16(int c)
{
    g(c);
    g(c >> 8);
}

/* An ADL address or immediate: three bytes, little endian. */
static void gen_le24(int c)
{
    g(c);
    g(c >> 8);
    g(c >> 16);
}

/* A three-byte field that the linker fills in. The addend travels in the
 * relocation rather than the field, because this target uses RELA. */
static void gen_addr24(Sym *sym, int addend)
{
    if (nocode_wanted)
        return;
    if (sym)
        greloca(cur_text_section, sym, ind, R_Z80_24, addend);
    gen_le24(sym ? 0 : addend);
}

/* The symbol standing for the text section itself, so that a jump inside a
 * function can be written as an absolute address the linker resolves. The
 * eZ80's jp takes an absolute target and there is no pc-relative form wide
 * enough to reach across a function, so every forward jump needs this. */
static Section *text_sym_sec;
static Sym text_sym;
static Sym *text_section_sym(void)
{
    /* Keyed on the section rather than made once: cur_text_section changes
     * between translation units and for a function placed in a section of its
     * own, and a symbol cached from the previous one names the wrong thing.
     * greloca reads only sym->c, so a bare Sym carrying the index is enough. */
    if (text_sym_sec != cur_text_section) {
        text_sym_sec = cur_text_section;
        memset(&text_sym, 0, sizeof text_sym);
        text_sym.c = put_elf_sym(symtab_section, 0, 0,
                                 ELFW(ST_INFO)(STB_LOCAL, STT_SECTION), 0,
                                 cur_text_section->sh_num, NULL);
    }

    return &text_sym;
}

/* ------------------------------------------------------------------ */
/* instructions, named the way the assembler spells them               */

static void ld_rr_imm(int r, int v)          /* ld rr, nn */
{
    g(0x01 + pp_of[r] * 0x10);
    gen_le24(v);
}

static void ld_rr_sym(int r, Sym *sym, int addend)
{
    g(0x01 + pp_of[r] * 0x10);
    gen_addr24(sym, addend);
}

static void ld_rr_ix(int r, int d)           /* ld rr, (ix+d) */
{
    o(0xdd);
    g(0x07 + pp_of[r] * 0x10);
    g(d);
}

static void ld_ix_rr(int d, int r)           /* ld (ix+d), rr */
{
    o(0xdd);
    g(0x0f + pp_of[r] * 0x10);
    g(d);
}

static void ld_rr_ind(int r, int addr_r)     /* ld rr, (hl) -- addr_r must be HL */
{
    o(0xed);
    g(0x07 + pp_of[r] * 0x10);
}

static void ld_ind_rr(int addr_r, int r)     /* ld (hl), rr */
{
    o(0xed);
    g(0x0f + pp_of[r] * 0x10);
}

static void ld_rr_abs(int r, Sym *sym, int addend)  /* ld rr, (nn) */
{
    if (r == TREG_HL) {
        g(0x2a);
    } else {
        o(0xed);
        g(0x4b + pp_of[r] * 0x10);
    }
    gen_addr24(sym, addend);
}

static void ld_abs_rr(Sym *sym, int addend, int r)  /* ld (nn), rr */
{
    if (r == TREG_HL) {
        g(0x22);
    } else {
        o(0xed);
        g(0x43 + pp_of[r] * 0x10);
    }
    gen_addr24(sym, addend);
}

static void push_rr(int r)  { g(0xc5 + pp_of[r] * 0x10); }
static void pop_rr(int r)   { g(0xc1 + pp_of[r] * 0x10); }

/* IY is the indirection register.
 *
 * Only HL can be a base for `ld rr,(hl)`, so dereferencing an address held in
 * DE or BC would mean moving it to HL -- and HL very often holds something
 * the register allocator still wants. `*a = *b` is the case that shows it:
 * the destination address sits in HL, evaluating *b moves b into HL on top of
 * it, and the store lands in the wrong object.
 *
 * IY costs a prefix byte and solves it outright. The backend reserves it, so
 * nothing tcc tracks ever lives there, and it can be used freely as scratch.
 */
static void pop_iy(void)  { o(0xfd); g(0xe1); }

static void mov_iy_rr(int r)                 /* iy = rr */
{
    push_rr(r);
    pop_iy();
}

static void ld_iy_ix(int d)                  /* ld iy, (ix+d) */
{
    o(0xdd);
    g(0x31);
    g(d);
}

static void ld_rr_iy(int r)                  /* ld rr, (iy+0) */
{
    o(0xfd);
    g(0x07 + pp_of[r] * 0x10);
    g(0);
}

static void ld_iy_rr(int r)                  /* ld (iy+0), rr */
{
    o(0xfd);
    g(0x0f + pp_of[r] * 0x10);
    g(0);
}

static void lea_rr_ix(int r, int d)          /* lea rr, ix+d */
{
    o(0xed);
    g(0x02 + pp_of[r] * 0x10);
    g(d);
}

static void add_hl_rr(int r) { g(0x09 + pp_of[r] * 0x10); }        /* add hl, rr */
static void sbc_hl_rr(int r) { o(0xed); g(0x42 + pp_of[r] * 0x10); }
static void or_a_a(void)     { g(0xb7); }
static void ex_de_hl(void)   { g(0xeb); }

/* Moving between register pairs: there is no ld rr, rr' on this chip.
 *
 * Always through the stack, never `ex de, hl`. That instruction is a byte
 * shorter but it swaps, and the register allocator is entitled to believe the
 * source still holds what it held. */
static void mov_rr(int dst, int src)
{
    if (dst == src)
        return;
    push_rr(src);
    pop_rr(dst);
}

/* ------------------------------------------------------------------ */
/* the displacement in (ix+d) is one signed byte                        */

static int fits_disp(int d)
{
    return d >= -128 && d <= 127;
}

static void need_disp(int d)
{
    if (!fits_disp(d))
        tcc_error("ez80: frame offset %d is out of range for (ix+d); "
                  "the frame must stay within 128 bytes of ix", d);
}

/* ------------------------------------------------------------------ */
/* load and store                                                      */

static int type_bytes(int t)
{
    int bt = t & VT_BTYPE;

    if (bt == VT_BYTE || bt == VT_BOOL)
        return 1;
    if (bt == VT_SHORT)
        return 2;
    if (bt == VT_PTR || bt == VT_FUNC || bt == VT_STRUCT)
        return PTR_SIZE;
    if (bt == VT_INT)
        return (t & VT_LONG) ? LONG_SIZE : INT_SIZE;

    return 0;   /* a size this backend does not handle yet */
}

static void unsupported(const char *what, int t)
{
    tcc_error("ez80: %s of this type is not implemented yet (btype %d)",
              what, t & VT_BTYPE);
}

ST_FUNC void load(int r, SValue *sv)
{
    int fr = sv->r;
    int ft = sv->type.t;
    int fc = sv->c.i;
    int v = fr & VT_VALMASK;
    int size = type_bytes(ft);

    if (fr & VT_LVAL) {
        /* The value lives in memory; fr says where its address comes from. */
        if (size != PTR_SIZE)
            unsupported("load", ft);

        if (v == VT_LLOCAL) {
            /* Two steps: the address itself is in a local slot. It goes to IY
             * rather than through a register, so nothing live is disturbed. */
            need_disp(fc);
            ld_iy_ix(fc);
            ld_rr_iy(r);

            return;
        }

        if (v == VT_LOCAL) {
            need_disp(fc);
            ld_rr_ix(r, fc);
        } else if (v == VT_CONST) {
            ld_rr_abs(r, (fr & VT_SYM) ? sv->sym : NULL, fc);
        } else if (v < NB_REGS) {
            /* The address is in a register. c.i is not a displacement here:
             * it still holds whatever the value had before gv() put it in a
             * register, and adding it read six bytes past every parameter. */
            if (v == TREG_HL) {
                ld_rr_ind(r, TREG_HL);
            } else {
                mov_iy_rr(v);
                ld_rr_iy(r);
            }
        } else {
            tcc_error("ez80: cannot load from value location %d", v);
        }

        return;
    }

    /* The value itself, not something at an address. */
    if (v == VT_CONST) {
        if (fr & VT_SYM)
            ld_rr_sym(r, sv->sym, fc);
        else
            ld_rr_imm(r, fc);
    } else if (v == VT_LOCAL) {
        need_disp(fc);
        lea_rr_ix(r, fc);
    } else if (v == VT_CMP) {
        /* A comparison left its answer in the flags, and vset_VT_JMP has put
         * the comparison token in c.i. Turn it into a 0 or a 1.
         *
         * Written as a jump over an assignment rather than with a setcc,
         * which this chip does not have. jp rather than jr because two of the
         * six conditions are sign conditions and jr cannot test those. */
        int skip;

        ld_rr_imm(r, 1);
        skip = gjmp_cond(fc, 0);
        ld_rr_imm(r, 0);
        gsym(skip);
    } else if (v == VT_JMP || v == VT_JMPI) {
        /* The value is whether control arrived here by the jump or by falling
         * through, so each path assigns the constant it stands for. */
        int t = v & 1;
        int skip;

        ld_rr_imm(r, t);
        skip = gjmp(0);
        gsym(fc);
        ld_rr_imm(r, t ^ 1);
        gsym(skip);
    } else if (v < NB_REGS) {
        mov_rr(r, v);
    } else {
        tcc_error("ez80: cannot load from value location %d", v);
    }
}

ST_FUNC void store(int r, SValue *v)
{
    int fr = v->r;
    int ft = v->type.t;
    int fc = v->c.i;
    int vt = fr & VT_VALMASK;
    int size = type_bytes(ft);

    if (size != PTR_SIZE)
        unsupported("store", ft);

    if (vt == VT_LLOCAL) {
        /* The destination address is itself in a local slot. */
        need_disp(fc);
        ld_iy_ix(fc);
        ld_iy_rr(r);

        return;
    }

    if (vt == VT_LOCAL) {
        need_disp(fc);
        ld_ix_rr(fc, r);
    } else if (vt == VT_CONST) {
        ld_abs_rr((fr & VT_SYM) ? v->sym : NULL, fc, r);
    } else if (vt < NB_REGS) {
        /* The destination address is in a register. Through IY unless it is
         * already in HL, so that neither the value nor anything else the
         * allocator is holding has to move. */
        if (vt == TREG_HL) {
            ld_ind_rr(TREG_HL, r);
        } else {
            mov_iy_rr(vt);
            ld_iy_rr(r);
        }
    } else {
        tcc_error("ez80: cannot store to value location %d", vt);
    }
}

/* ------------------------------------------------------------------ */
/* calls                                                               */

/* call <sym> */
static void call_sym(Sym *sym)
{
    g(0xcd);
    gen_addr24(sym, 0);
}

static void call_helper(int tok)
{
    call_sym(external_helper_sym(tok));
}

/* ------------------------------------------------------------------ */
/* function calls and frames                                           */

/* Arguments occupy whole 3-byte slots, which is what agondev does. */
static int arg_slot(int size)
{
    return (size + PTR_SIZE - 1) / PTR_SIZE * PTR_SIZE;
}

/* Discard n bytes of arguments after a call, without disturbing HL -- the
 * return value is in it. */
static void gadd_sp(int n)
{
    if (n == 0)
        return;

    if (n <= 8 * PTR_SIZE) {
        /* One byte each, and it only costs DE. This is what agondev emits. */
        int i;

        for (i = 0; i < n / PTR_SIZE; i++)
            pop_rr(TREG_DE);

        return;
    }

    /* Bigger: go through DE so that HL survives. */
    ex_de_hl();
    ld_rr_imm(TREG_HL, n);
    g(0x39);                  /* add hl, sp */
    g(0xf9);                  /* ld sp, hl */
    ex_de_hl();
}

ST_FUNC void gfunc_call(int nb_args)
{
    int args_size = 0;
    int i, r, size, align;

    save_regs(nb_args + 1);

    /* Pushed from vtop down, which puts the first argument at the lowest
     * address -- the layout agondev's own code produces. */
    for (i = 0; i < nb_args; i++) {
        size = type_size(&vtop->type, &align);
        if ((vtop->type.t & VT_BTYPE) == VT_STRUCT)
            unsupported("passing a struct by value", vtop->type.t);
        if (type_bytes(vtop->type.t) != PTR_SIZE)
            unsupported("argument", vtop->type.t);
        r = gv(RC_INT);
        push_rr(r);
        args_size += arg_slot(size);
        vtop--;
    }

    save_regs(0);

    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST && (vtop->r & VT_SYM)) {
        call_sym(vtop->sym);
    } else {
        gv(RC_HL);
        call_helper(TOK__indcallhl);
    }

    /* The caller cleans up. */
    gadd_sp(args_size);
    vtop--;
}

/* ld hl, -size  (4 bytes) then call __frameset (4 bytes). The size is not
 * known until the body has been compiled, so the space is reserved here and
 * filled in by gfunc_epilog. */
#define FUNC_PROLOG_SIZE 8

ST_FUNC void gfunc_prolog(Sym *func_sym)
{
    CType *func_type = &func_sym->type;
    int addr, align, size;
    Sym *sym;
    CType *type;

    sym = func_type->ref;

    /* __frameset leaves ix pointing at the saved ix. Above it sit the return
     * address and then the arguments, so the first one is at ix+6. */
    addr = 2 * PTR_SIZE;
    loc = 0;
    func_vc = 0;

    ind += FUNC_PROLOG_SIZE;
    func_sub_sp_offset = ind;

    /* A struct return is written through a hidden first argument. */
    if ((func_vt.t & VT_BTYPE) == VT_STRUCT) {
        func_vc = addr;
        addr += PTR_SIZE;
    }

    while ((sym = sym->next) != NULL) {
        type = &sym->type;
        size = type_size(type, &align);
#ifdef FUNC_STRUCT_PARAM_AS_PTR
        if ((type->t & VT_BTYPE) == VT_STRUCT)
            size = PTR_SIZE;
#endif
        gfunc_set_param(sym, addr, 0);
        addr += arg_slot(size);
    }

    func_ret_sub = 0;
}

ST_FUNC void gfunc_epilog(void)
{
    int v, saved_ind;

    /* Locals were allocated by moving sp, so sp is restored from ix. Doing it
     * unconditionally costs two bytes in a function with no locals and avoids
     * the epilogue depending on a size that is only known here. */
    o(0xf9dd);          /* ld sp, ix */
    o(0xe1dd);          /* pop ix */
    g(0xc9);            /* ret */

    v = (-loc + PTR_SIZE - 1) / PTR_SIZE * PTR_SIZE;

    saved_ind = ind;
    ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
    ld_rr_imm(TREG_HL, -v);
    call_helper(TOK__frameset);
    if (ind != func_sub_sp_offset)
        tcc_internal_error("ez80: prologue is not FUNC_PROLOG_SIZE bytes");
    ind = saved_ind;
}

/* ------------------------------------------------------------------ */
/* jumps                                                               */

/* Z80 condition codes, as jp cc,nn encodes them. */
#define CC_NZ 0
#define CC_Z  1
#define CC_NC 2
#define CC_C  3
#define CC_PO 4
#define CC_PE 5
#define CC_P  6
#define CC_M  7

/* A jump whose target is not known yet. The three address bytes carry the
 * link to the previous jump in the chain until gsym_addr patches them, which
 * is when the relocation is created -- the field itself is never read by the
 * linker, because this target uses RELA and the address travels in the
 * relocation's addend. */
static int gjmp_chain(int opcode, int t)
{
    int r;

    if (nocode_wanted)
        return t;
    g(opcode);
    r = ind;
    gen_le24(t);

    return r;
}

ST_FUNC int gjmp(int t)
{
    return gjmp_chain(0xc3, t);
}

ST_FUNC void gjmp_addr(int a)
{
    g(0xc3);
    gen_addr24(text_section_sym(), a);
}

ST_FUNC void gsym_addr(int t, int a)
{
    while (t) {
        unsigned char *ptr = cur_text_section->data + t;
        int n = ptr[0] | ptr[1] << 8 | ptr[2] << 16;   /* next in the chain */

        /* put_elf_reloca rather than greloca, which drops a relocation while
         * nocode_wanted is set. A forward jump is very often patched in
         * exactly that state: the code after an unconditional jump is
         * unreachable, so nocode_wanted is on, and gsym only clears it after
         * calling here. The jump itself was emitted while the code was still
         * live and is certainly real, so its relocation has to be too. */
        put_elf_reloca(symtab_section, cur_text_section, t, R_Z80_24,
                       text_section_sym()->c, a);
        ptr = cur_text_section->data + t;             /* refetch: sections move */
        ptr[0] = ptr[1] = ptr[2] = 0;
        t = n;
    }
}

ST_FUNC int gjmp_append(int n, int t)
{
    if (n) {
        /* Walk to the end of chain n and splice t onto it. */
        int p = n, next;

        for (;;) {
            unsigned char *ptr = cur_text_section->data + p;
            next = ptr[0] | ptr[1] << 8 | ptr[2] << 16;
            if (!next)
                break;
            p = next;
        }
        {
            unsigned char *ptr = cur_text_section->data + p;
            ptr[0] = t;
            ptr[1] = t >> 8;
            ptr[2] = t >> 16;
        }
        t = n;
    }

    return t;
}

/* Maps a comparison token onto the condition that reads it out of the flags
 * left by `or a,a; sbc hl,de`. The four that have no condition of their own
 * are turned into their mirror image in gen_opi, by swapping the operands. */
static int cc_of_op(int op)
{
    switch (op) {
        case TOK_EQ:  return CC_Z;
        case TOK_NE:  return CC_NZ;
        case TOK_ULT: return CC_C;    /* sbc set the borrow */
        case TOK_UGE: return CC_NC;
        case TOK_LT:  return CC_M;    /* after __setflag repairs S */
        case TOK_GE:  return CC_P;
    }

    tcc_error("ez80: no condition code for comparison %d", op);

    return 0;
}

ST_FUNC int gjmp_cond(int op, int t)
{
    return gjmp_chain(0xc2 + cc_of_op(op) * 8, t);
}

ST_FUNC void ggoto(void)
{
    /* An indirect jump: the target is a value, so it has to go through a
     * register. jp (hl) is the only indirect jump this chip has. */
    gv(RC_HL);
    o(0xe9);          /* jp (hl) */
    vtop--;
}

ST_FUNC void gen_fill_nops(int bytes)
{
    while (bytes--)
        g(0x00);      /* nop */
}

/* ------------------------------------------------------------------ */
/* integer operations                                                  */

/* Every helper takes its right operand in BC and returns in HL. */
static void gen_op_helper(int tok)
{
    gv2(RC_HL, RC_BC);
    call_helper(tok);
    vtop--;
    vtop->r = TREG_HL;
}

ST_FUNC void gen_opi(int op)
{
    int t = vtop->type.t;
    int uns = (t & VT_UNSIGNED) != 0;

    if (type_bytes(t) != PTR_SIZE)
        unsupported("arithmetic", t);

    switch (op) {
    case '+':
        gv2(RC_HL, RC_DE);
        add_hl_rr(TREG_DE);
        vtop--;
        vtop->r = TREG_HL;
        break;

    case '-':
        gv2(RC_HL, RC_DE);
        or_a_a();                 /* clear carry; sbc uses it */
        sbc_hl_rr(TREG_DE);
        vtop--;
        vtop->r = TREG_HL;
        break;

    case '*':      gen_op_helper(uns ? TOK__imulu : TOK__imuls); break;
    case '&':      gen_op_helper(TOK__iand); break;
    case '|':      gen_op_helper(TOK__ior);  break;
    case '^':      gen_op_helper(TOK__ixor); break;
    case TOK_SHL:  gen_op_helper(TOK__ishl); break;
    case TOK_SAR:  gen_op_helper(TOK__ishrs); break;
    case TOK_SHR:  gen_op_helper(TOK__ishru); break;
    case '/':      gen_op_helper(uns ? TOK__idivu : TOK__idivs); break;
    case TOK_UDIV: gen_op_helper(TOK__idivu); break;
    case '%':      gen_op_helper(uns ? TOK__iremu : TOK__irems); break;
    case TOK_UMOD: gen_op_helper(TOK__iremu); break;

    case TOK_EQ: case TOK_NE:
    case TOK_ULT: case TOK_UGE: case TOK_ULE: case TOK_UGT:
    case TOK_LT: case TOK_GE: case TOK_LE: case TOK_GT: {
        int signed_cmp;

        /* Four of the ten have no condition code of their own. Each is the
         * mirror of one that does -- a > b is b < a -- so the operands are
         * swapped and the mirrored comparison used. */
        switch (op) {
            case TOK_ULE: vswap(); op = TOK_UGE; break;
            case TOK_UGT: vswap(); op = TOK_ULT; break;
            case TOK_LE:  vswap(); op = TOK_GE;  break;
            case TOK_GT:  vswap(); op = TOK_LT;  break;
        }
        signed_cmp = (op == TOK_LT || op == TOK_GE);

        gv2(RC_HL, RC_DE);
        /* The comparison has to leave HL as it found it. x86 compares with
         * an instruction that does not write back; the eZ80 subtracts, and
         * tcc counts on the left operand surviving -- a switch loads the
         * value once and compares it against every case in turn, so a
         * destructive compare tests `x`, then `x-1`, then `x-1-2`.
         *
         * push and pop do not touch the flags, and __setflag only uses BC and
         * AF, so the saved value can be restored after it runs. */
        push_rr(TREG_HL);
        or_a_a();
        sbc_hl_rr(TREG_DE);
        if (signed_cmp) {
            /* sbc leaves S wrong when the subtraction overflowed, and the
             * chip signals that in P/V. __setflag repairs S, and is called
             * only when it has to be. */
            g(0xec);                        /* call pe, nn */
            gen_addr24(external_helper_sym(TOK__setflag), 0);
        }
        pop_rr(TREG_HL);
        vtop--;
        /* vset_VT_CMP and not the two fields by hand: jtrue and jfalse share
         * storage with c.i, so assigning only r and cmp_op leaves whichever
         * constant was in c.i looking like a pending jump chain. That is not
         * theoretical -- `n > 1` left a 1 there, and gvtst then tried to
         * patch a jump at offset 1, in the middle of the prologue. */
        vset_VT_CMP(op);
        break;
    }

    default:
        tcc_error("ez80: integer operation %d is not implemented yet", op);
    }
}

/* ------------------------------------------------------------------ */
/* everything below is still to do                                     */

ST_FUNC void gen_opf(int op)
{
    tcc_error("ez80: floating point is not implemented yet");
}

ST_FUNC void gen_cvt_itof(int t)
{
    tcc_error("ez80: integer to float conversion is not implemented yet");
}

ST_FUNC void gen_cvt_ftoi(int t)
{
    tcc_error("ez80: float to integer conversion is not implemented yet");
}

ST_FUNC void gen_cvt_ftof(int t)
{
    tcc_error("ez80: float conversion is not implemented yet");
}

ST_FUNC void gen_vla_sp_save(int addr)
{
    tcc_error("ez80: variable length arrays are not implemented yet");
}

ST_FUNC void gen_vla_sp_restore(int addr)
{
    tcc_error("ez80: variable length arrays are not implemented yet");
}

ST_FUNC void gen_vla_alloc(CType *type, int align)
{
    tcc_error("ez80: variable length arrays are not implemented yet");
}

/* Struct return: measured, a struct of any size goes through a hidden pointer
 * passed as the first argument, and that pointer comes back in HL. */
ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ralign, int *regsize)
{
    *ralign = 1;
    *regsize = PTR_SIZE;

    return 0; /* always through memory */
}

/******************************************************/
#endif /* ! TARGET_DEFS_ONLY */
/******************************************************/
