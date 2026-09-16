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

/* Everything below is the code generator, and none of it is written yet.
 *
 * The frontend is complete without it: preprocessing, parsing, type checking,
 * sizeof and struct layout all run on the target data model above, which is
 * what the cross-compiler is for at this stage. Reaching any of these means a
 * translation unit got as far as emitting code, so they say so rather than
 * emitting something wrong and quietly producing a broken binary. */
static void ez80_todo(const char *what)
{
    tcc_error("ez80 backend: %s is not implemented yet", what);
}

ST_FUNC void gsym_addr(int t, int a) { ez80_todo("gsym_addr"); }
ST_FUNC void load(int r, SValue *sv) { ez80_todo("load"); }
ST_FUNC void store(int r, SValue *v) { ez80_todo("store"); }
ST_FUNC void gfunc_call(int nb_args) { ez80_todo("gfunc_call"); }
ST_FUNC void gfunc_prolog(Sym *func_sym) { ez80_todo("gfunc_prolog"); }
ST_FUNC void gfunc_epilog(void) { ez80_todo("gfunc_epilog"); }
ST_FUNC int gjmp(int t) { ez80_todo("gjmp"); return 0; }
ST_FUNC void gjmp_addr(int a) { ez80_todo("gjmp_addr"); }
ST_FUNC int gjmp_cond(int op, int t) { ez80_todo("gjmp_cond"); return 0; }
ST_FUNC int gjmp_append(int n, int t) { ez80_todo("gjmp_append"); return 0; }
ST_FUNC void gen_opi(int op) { ez80_todo("gen_opi"); }
ST_FUNC void gen_opf(int op) { ez80_todo("gen_opf"); }
ST_FUNC void gen_cvt_itof(int t) { ez80_todo("gen_cvt_itof"); }
ST_FUNC void gen_cvt_ftoi(int t) { ez80_todo("gen_cvt_ftoi"); }
ST_FUNC void gen_cvt_ftof(int t) { ez80_todo("gen_cvt_ftof"); }
ST_FUNC void ggoto(void) { ez80_todo("ggoto"); }
ST_FUNC void gen_vla_sp_save(int addr) { ez80_todo("gen_vla_sp_save"); }
ST_FUNC void gen_vla_sp_restore(int addr) { ez80_todo("gen_vla_sp_restore"); }
ST_FUNC void gen_vla_alloc(CType *type, int align) { ez80_todo("gen_vla_alloc"); }
ST_FUNC void gen_fill_nops(int bytes) { ez80_todo("gen_fill_nops"); }
ST_FUNC void o(unsigned int c) { ez80_todo("o"); }

/* Struct return: measured, a struct of any size goes through a hidden pointer
 * argument. Reporting that here rather than in the unwritten codegen keeps
 * the frontend's struct layout honest, and it is the answer for every size. */
ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ralign, int *regsize)
{
    *ralign = 1;
    *regsize = PTR_SIZE;

    return 0; /* always through memory */
}

/******************************************************/
#endif /* ! TARGET_DEFS_ONLY */
/******************************************************/
