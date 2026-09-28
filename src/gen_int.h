/*
 * What the code generator's files share among themselves. What the parser
 * and the rest of acc see of them is in acc.h; this is their state -- the
 * value stack, the function being compiled, the tables the end of a file
 * settles -- and their calls to each other.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_GEN_INT_H
#define ACC_GEN_INT_H

#include "acc.h"
#include "rt_helpers.h"

/* That a frame offset fits the signed byte of (ix+d). Tested inline, because
 * it runs for every local touched and the call to test it opened a frame; as
 * one unsigned compare rather than two signed ones, because a signed compare
 * on this target is a call to repair the flags. */
#define disp_fits(d)  ((unsigned) ((d) + 128) <= 255u)

#define STEP_MAX 4             /* inc/dec, before ld bc,n and add wins */

#define MUL_MAX_STEPS 12        /* doublings plus additions, before it is
                                 * cheaper to let the helper do it */

/* agondev hands a one-byte result back in A and everything else in HL, and
 * acc matches it -- not as a courtesy but because acc exists so that code can
 * be compiled on the machine, and a library built with agondev has to keep
 * working. test/abi.sh pins the convention; there is no document that states
 * it. A is genuinely a different register rather than a narrower read of HL,
 * which makes this the one place the two sides could quietly disagree. */
#define RETURNS_IN_A(ty) (type_size(ty) == 1)

/* How many values an expression may have pending at once. A call's
 * arguments are all pending until the call is made, and C99 5.2.4.1 asks
 * for 127 of them -- 64 here refused YARPGen's drivers, which pass a test
 * function 70 to 90 -- with room above them for what the last one nests. */
#define VSTACK_MAX 256

/* Slots handed out that the value stack does not point at yet: see
 * spill_free_from. */
#define SPILL_PENDING 4

typedef struct {
    int disp, end;
} PendingSpill;

typedef struct {
    int at;                      /* the hole: the operand of an ld de, nn */
    int array;                   /* whose address it is */
} ArrayPatch;

/* Calls to functions that have not been compiled yet.
 *
 * A one-pass compiler cannot know where a function will be until it reads it,
 * so the call is emitted with a hole and the hole is remembered. There are as
 * many of these as there are forward calls, which is nothing beside keeping
 * the whole program in memory to make two passes over it. */
/*
 * Six bytes each on the Agon, and a link of acc itself makes six thousand.
 * Every call is to a function already declared -- one that is not is
 * refused where it is called -- so there is nothing about the call to keep
 * but where it is. At the end, a fixup whose slot has gone to the image's
 * file becomes, in its own place, the addition the file is to get (OutAdd,
 * which it is laid out as). */
typedef struct {
    int fn;                 /* index, not a pointer: see sym.c */
    int at;
} Fixup;

typedef char fixup_is_an_add[sizeof(Fixup) == sizeof(OutAdd) ? 1 : -1];


/* jp cc, nn -- the condition codes this file uses. */
#define JP_ANY  0xc3

#define JP_Z    0xca

#define JP_NZ   0xc2

#define JP_PE   0xea            /* overflow */

#define JP_P    0xf2            /* sign clear */

#define JP_M    0xfa            /* sign set */

#define JP_C    0xda            /* carry set: a borrow, so unsigned less */

#define JP_NC   0xd2            /* and clear, so unsigned not less */

/* The wide constants a function's routines read, laid down once after its
 * code: an operand that is a constant is ld de, its address, four bytes,
 * where writing it into a scratch slot was sixteen for a long, and was done
 * again at every use. Each use is recorded, and its address filled in when
 * the pool is laid down at the function's end; jumps shortened before
 * that move the uses, and a rewind takes back the ones it passes. */
#define POOL_MAX 32

typedef struct {
    unsigned char which;
    int at;
} RtFixup;

/* Where each of the lists acc keeps stood when a function began, so that
 * shortening that function's jumps walks its own entries and not the ones
 * every function before it left. Everything recorded before the first run is
 * where it was. */
typedef struct {
    int reloc, fixup, rt, bss, jump;
} Mark;

/* insn.c */
extern const unsigned char reg_code[NREGS];
extern int imm_hl_end;
extern unsigned imm_hl_epoch;
void ld_rr_imm(int reg, int imm) /* ld rr, nn */;
extern int stored_at, stored_disp, stored_reg, join_at;
extern unsigned stored_epoch;
void ld_ix_rr(int disp, int reg) /* ld (ix+d), rr */;
void push_rr(int reg);
void pop_rr(int reg);
void add_hl_rr(int reg);
void sbc_hl_rr(int reg);
void or_a_a(void);
void mov_rr(int dst, int src);
int far_base(int disp);
__attribute__((noinline, noreturn)) void void_used(void);
__attribute__((noinline, noreturn)) void struct_used(void);
void frame_byte(int op, int disp);
void ld_a_ix(int disp);
void ld_e_ix(int disp);
void ld_l_ix(int disp);
void ld_ix_a(int disp);
void ld_l_a(void);
void ld_h_a(void);
void ld_a_l(void);
void ld_a_h(void);
void ld_a_hl(void);
void ld_hl_a(void);
void inc_hl(void);
void dec_hl(void);
void inc_de(void);
void ld_de_a(void);
void ex_de_hl(void);
void ld_hl_ind_hl(void);
void ld_ind_hl_de(void);
void sbc_hl_hl(void);
void fill_hl_with_sign_of_a(void);
void fill_hl_with_zero(void);
int and_a_imm(int v);
int or_a_imm(int v);
int xor_a_imm(int v);
void add_hl_hl(void);
extern const unsigned powers_of_two[16];
void load_narrow_into(int reg, int disp, Type type);
void store_narrow(int disp, Type type);

/* vstack.c */
extern Type return_type;
extern int return_ext;
extern Value vstack[VSTACK_MAX];
extern int vtop;
extern Value *vsp;
void check_reg_free(int reg);
extern int locals_size;
extern int spill_used;
extern PendingSpill spill_pending[SPILL_PENDING];
extern unsigned char nspill_pending;
extern int spill_peak;
extern int spill_locked;
extern int frame_patch;
extern int frame_call;
extern int arrays_size;
extern int *array_end;
extern int narrays, array_end_cap;
extern ArrayPatch *array_patches;
extern int narray_patches, array_patches_cap;
int frame_size(void);
void vcheck(void);
void vset_width(Value *v, int width);
extern int nwide_consts;
uint64_t wide_value(const Value *v);
void wide_needs_slot(void);
int wide_table_of(const Value *a, const Value *b);
int vconst_pair(void);
uint64_t const_as(const Value *v, Type to);
int wide_push(uint64_t bits, Type type, int ext);
void save_regs_below(int n);
int reg_alloc(void);
int reg_alloc_other(int avoid);
int force_reg(Value *val);
void evict_reg(int reg);
void force_into(Value *target, int want);

/* arith.c */
int trunc_int(int value);
extern int gaddr_end, gaddr_reg;
extern unsigned gaddr_epoch;
void vbinop(int op);
extern int conversion_from;
extern int conversion_to;
extern unsigned conversion_epoch;
void ld_a_imm(int value);
extern int cmp_from;
extern int cmp_to;
extern int cmp_op;
extern int cmp_was_unsigned;
extern unsigned cmp_epoch;
void cmp_equal(int when_equal);
void cmp_unsigned(int when_borrow);
void cmp_signed(int when_negative);
void hl_zero_test(void);
void cmp_value(int op, int is_unsigned);
Type common_wide(Type left, Type right);

/* wide.c */
void lea_rr_ix(int reg, int disp);
void convert_int_to_float(void);
void convert_float_to_int(Type to);
void no_float_address(const Value *from);
void check_no_float_mix(Type to, const Value *from);
void materialise_long(int disp, Type type);
void wide_through_hl(int slot, int n, int store);
void wide_to_slot(uint64_t bits, Type type);
int long_scratch(Type type);
int spill_start_of(const Value *v, int *size);
int64_t wide_signed(uint64_t bits, int width);
void vunary_long(int which, Type type);
extern int pool_n[POOL_MAX], pool_addr[POOL_MAX], npool;
extern int *pool_site_at, *pool_site_entry, npool_sites, pool_sites_cap;
void pool_emit(void);
void gen_rewound(int here);
int long_into(int offset, Type type);
void vbinop_long(int op, Type result);
void vcmp_wide(int op, Type operand);

/* branch.c */
extern int in_function;
extern int *jump_at, *jump_put;
extern unsigned char *jump_cc, *jump_cc_put;
extern int njumps, jumps_cap;
void jumps_rewind(int n);
int jump_op(int op);
void jumps_forget(int here);
void patch_to_here(int hole);
extern int widen_from;
void widen_loaded(Type to);
int widen_undo(const Value *v);
int widen_holds(const Value *v, Type *type);
int widen_undo_left(void);
int store_byte_widened(int offset, Type type);
int widen_again(Type to);
extern int logic_from;
__attribute__((noinline)) void bool_from(void);

/* runtime.c */
extern int rt_nused;
void rt_unwant_to(int n);
extern RtFixup *rt_fixups;
extern int nrt_fixups, rt_fixups_cap;
extern int rt_syms[RT_COUNT];
void rt_syms_init(void);
int rt_which(const char *name);
void rt_wanted(int which);
int rt_symbol(int which);
int needs_helper(int op);
void rt_call(int which);
void rt_emit_used(void);

/* finish.c */
/* Kept in blocks of FIXUP_BLOCK, and a list of the blocks, rather than in
 * one array: a link of acc made six thousand, and an array doubling
 * towards them wanted 74 KB in one piece when the Agon's heap had nothing
 * that size left. A block is 1.5 KB, and finding one is a byte of the
 * index; the rest of the index is the place in it. */
#define FIXUP_BLOCK 256

extern Fixup **fixup_blocks;
extern int nfixups;

/* The i-th, or null past the last block. Out of line, and asked only at
 * the start of a block: finding one scales an index by entries that are
 * not a power of two wide, which is a multiply, and a multiply is a call.
 * Within a block, a walk steps a pointer (FIXUP_STEP). */
Fixup *fixup_at(int i);

/* The place of the i-th, the one before it being at `f`: the next in the
 * block, or the start of the next block. `i` is read twice, so it is a
 * variable and not `++n`: at a block's start that counted twice. */
#define FIXUP_STEP(f, i)  ((unsigned char) (i) ? (f) + 1 : fixup_at(i))
void fixup_add(int fn, int at);
/* In blocks of 256, as the fixups are and for the same reason: a link of
 * acc itself has seven thousand. */
extern int **bss_blocks;
extern int nbss_fixups;

int *bss_fixup(int i);                  /* as fixup_at, and BSS_STEP */
#define BSS_STEP(p, i)    ((unsigned char) (i) ? (p) + 1 : bss_fixup(i))
int no_address(int sym);
int exit_cell(int which);

/* relax.c */
void want(int fn);
void static_begin(int fn);
void static_end(void);
extern Mark func_mark;
void mark_here(Mark *m);
extern Cut *relax_cuts;
extern int *relax_target, *relax_slot;
extern unsigned char *relax_short;
extern int relax_cap;
extern int *arr_cut_at, narr_cuts, arr_cuts_cap;
void relax_function(const Mark *from, int frame_at);
void drop_unused_statics(void);

/* func.c */
int spill_slot_of(int size);
int spill_slot(void);
void vpush_scratch(Type type, int slot);

/* lvalue.c */
void vcmp_pointer_check(Type left, Type right);
void vbinop_pointer(int op, Type left, Type right);

static inline __attribute__((always_inline))
void ld_rr_ix(int reg, int disp)           /* ld rr, (ix+d) */
{
    if (stored_at == out_here() && stored_disp == disp && stored_reg == reg
        && stored_epoch == out_rewinds && join_at != stored_at)
        return;
    if (disp_fits(disp))
        out_byte3(0xdd, 0x07 + reg_code[reg], disp);
    else
        out_byte3(0xfd, 0x07 + reg_code[reg], far_base(disp));
}

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

/* How many of an int's bytes can be other than zero: 1, 2 or 3.
 *
 * On this chip an AND, OR or XOR of two ints is a call, since the top byte
 * of a register has no name to do it with. But a byte that is zero is zero
 * whatever the operator does to it and to another zero -- and the ints
 * that are made of an unsigned char, which is what C promotes every one
 * to, have two of them: zap's register masks are `uint8_t`, and every
 * operator on them was a call. So a value can say what it is known to fit
 * in, in two bits of its quals, set where it is made -- a load of an
 * unsigned narrow type, an AND with something narrow -- and cleared by
 * everything else, since a push starts with none. A constant says it by
 * being the number it is. */
static inline __attribute__((always_inline)) int vwidth(const Value *v)
{
    if (val_number(v->kind))
        return v->val >= 0 && v->val <= 0xff ? 1
             : v->val >= 0 && v->val <= 0xffff ? 2 : 3;

    return v->quals & VQ_BYTE ? 1 : v->quals & VQ_WORD ? 2 : 3;
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

#endif
