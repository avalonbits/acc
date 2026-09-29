/*
 * Code generation: the values the parser pushes and the calls that
 * turn them into instructions. See the generator's files, and gen_int.h
 * for what they share among themselves.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_GEN_H
#define ACC_GEN_H

/* In opt-acc, the parser's calls into the code generator go through
 * genlog.c, which writes each one down as well as making it. The parser's
 * files say they are the parser (ACC_FRONT); the code generator's do not,
 * and call one another directly. */
#if defined(OPT_ACC) && defined(ACC_FRONT)
#define GENLOG_RENAME
#include "genlog_calls.h"
#undef GENLOG_RENAME
#endif

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "sym.h"

/* What the expression compiler pushes instead of emitting. A value is not
 * turned into instructions until something needs it in a register, so
 * `1 + 2` never reaches the code generator and `x + 1` loads x once. */
enum {
    VAL_CONST,      /* a literal; val holds it */
    VAL_ADDR,       /* and one that is an address inside the image: a
                     * global's, a string's, a compound literal's. Wherever
                     * one of these is written out it is a relocation, and
                     * this is how the code generator knows to record one --
                     * by the time the bytes are emitted there is nothing
                     * else left to say so.
                     *
                     * A kind of its own rather than a flag in a field of
                     * its own, because every place that asks "is this a
                     * constant?" has the kind in hand already -- and because
                     * a Value nine bytes wide is one whose width is not a
                     * power of two. The two measured the same, so this is
                     * the one that costs no room. It rides along with the
                     * value through folding, so `&a[3]` is still an
                     * address. */
    VAL_BSS,        /* and one that is an offset into the bss, which starts
                     * where the image ends. The two are told apart because
                     * neither is known as it is compiled and they are not
                     * known in the same way: an address inside the image
                     * moves with the image, and an offset into the bss has
                     * the start of the bss added to it. Folding works on
                     * both, which is the point of carrying the offset
                     * rather than loading the variable's address and adding
                     * to it: `a[3]` stays one load. */
    VAL_LOCAL,      /* a local at frame offset val */
    VAL_REG,        /* already in register val */
    VAL_ACC,        /* in A, still narrow: see the note on byte arithmetic */
    VAL_VOID,       /* what a void function returned, which is nothing */
    VAL_WIDE,       /* a constant too wide for val: see wide_const */
    VAL_IY,         /* the local that lives in IY, plus val: see iy_local */
    VAL_IYADDR      /* and where it is, which is nowhere: see vaddr_local */
};

/* Whether a value is a constant of any of the three kinds. VAL_CONST is
 * zero and the other two follow it, so this is the same single compare that
 * `kind == VAL_CONST` was before an address became a kind of its own. */
#define val_const(kind) ((unsigned) (kind) <= VAL_BSS)

/* Whether it is one whose value is not known as it is compiled: an address
 * inside the image, or an offset into the bss. Both fold, and both leave a
 * slot for someone to put right. */
#define val_pending(kind) ((unsigned) (kind) - VAL_ADDR <= 1u)

/* Whether `val` is the number the operand stands for, rather than where the
 * operand will be.
 *
 * val_const answers yes for an address and for a bss offset too, and their
 * `val` is a place and not a worth: the first thing in the bss is at offset
 * zero. Anything that compares `val` against a number wants this instead.
 * Reading that zero as the number zero is what made `p - first_array` throw
 * the subtraction away and compile to nothing but the divide. */
#define val_number(kind) ((kind) == VAL_CONST)

/* What the parser knows about a value it has not had to emit yet: a constant,
 * a local at a frame offset, or something already in a register. `val` is the
 * constant, the offset or the register number, according to `kind`. */
typedef struct {
    unsigned char kind;
    Type          type;
    int           val;
    unsigned char ext;          /* the type's extension, when it has one */
    unsigned char quals;        /* VQ_*: of what is at the bottom of the
                                 * chain of pointers, as ext is */
    unsigned char bits;         /* the address of a bit-field: which one, as
                                 * bitfield_at has it; 0 for anything else */
} Value;

enum {
    VQ_BYTE  = 0x40,            /* an int whose upper two bytes are zero */
    VQ_WORD  = 0x80,            /* one whose top byte is: see vwidth. Clear
                                 * of the SQ_* bits, which a local's value
                                 * takes as they are */
    VQ_CONST = 1                /* what the value leads to is const: a store
                                 * through it is refused */
};

/* ------------------------------------------------------------------ */
/* code generation                                                     */

/* The three 24-bit general registers in ADL mode. IX is the frame pointer and
 * IY is kept free as the backend's own scratch, which is what lets a value be
 * dereferenced without disturbing anything the allocator is holding. */
enum { R_HL = 0, R_DE, R_BC, NREGS };

void gen_init(void);
void gen_func_begin(int fn, int nparams, Type returns);

/* The one local of a function that lives in IY rather than in its frame
 * slot: the slot's offset, or 0 for none -- 0 is the saved IX, which no
 * local is. See gen_iy_claim. */
extern int iy_local;
extern int iy_any;                    /* test builds: the first that can, not
                                       * only a register one */
int  gen_iy_can(Type type);           /* whether a local of this type may */
void gen_iy_claim(int offset, Type type, int quals);
#ifdef OPT_ACC
/* prescan.c, opt-acc's: which local lives in IY, read from the body first */
extern NameRef iy_wants[4];           /* prescan_body's choices, best first */
extern int     iy_nwants;
int     inline_has(int fn);           /* inline.c */
void    prescan_begin(void);
void    prescan_param(NameRef name, int offset, int can);
int     prescan_body(const char *at, NameRef *wants, int *param);
void    prescan_claim(int offset, Type type, NameRef name);
void    gen_iy_take(int offset);     /* vstack.c: the one it chose */
#endif
int  gen_iy_pick(int offset, Type type, int quals);  /* a parameter */
void gen_iy_param(int offset);        /* and once the prologue is laid */
void gen_func_end(void);
extern int gen_effects;               /* side effects compiled so far */
int  gen_local(int size);
int  gen_local_fits(int size);        /* whether (ix+d) still reaches */
int  gen_local_far(int size);         /* one it does not: its array number */

/* An array whose length is not known until it runs: the room comes off the
 * stack where it is declared and goes back when the block ends.
 *
 * gen_stack_take reads the byte count from the top of the value stack,
 * takes that much, and leaves the address in the local at `slot`.
 * gen_stack_mark notes where the stack is, and gen_stack_back puts it
 * there again -- at the end of the block, and before a jump out of it. */
void gen_stack_take(int slot);
void gen_stack_mark(int slot);
void gen_stack_back(int slot);
int  gen_local_array(void);           /* a new local array; returns its number */
void gen_local_array_size(int array, int size);  /* and how big, once known */
void vaddr_array(int array, Type elem);  /* its first element's address */
void gen_zero_array(int array, int from, int size);  /* zero part of one */
int  gen_data(const char *bytes, int len);  /* a string's bytes: its address */
extern int gen_data_bytes;                  /* how many gen_data has written */
void gen_copy_to_array(int array, int offset, int from, int count);

void vpush_const(int val, Type type);
void vset_addr(void);                 /* the top is an address in the image */
void vpush_bss(int at, Type type);    /* and one that is `at` into the bss */
int  vconst_addr(void);               /* and whether it still is */
int  vconst_bss(void);                /* or an offset into the bss */
void vpush_const_long(long val, Type type);  /* four bytes, so it goes to the frame */
void vpush_const_wide(uint32_t low, uint32_t high, Type type);   /* eight */
void vpush_const_float(float val);
void vconvert(Type to);               /* narrow the top, then widen it back */
Type vtype(void);                     /* the type of the top */
int  vext(void);                      /* and its extension */
int  vconst_top(int *val, Type *type);
int  vconst_wide(uint64_t *bits, Type *type);   /* and one of four or eight
                                                 * bytes, as its bits */  /* whether the top is a constant */
void vset_type(Type type, int ext);   /* the same address, another pointer type */
void vset_ext(int ext);               /* the top's type's extension */
void vset_quals(int quals);           /* and its VQ_* */
void vset_bits(int bits);             /* it is a bit-field's address */
int  vbits(void);
int  vquals(void);
Type vtype_at(int depth);             /* 0 is the top, 1 the one below */
void vpush_local(int offset, Type type);
void vpush_reg(int reg);
void vstore_local(int offset, Type type); /* pop the top into a local */

/* Combine the top two values with the binary operator `op`. Which of the six
 * ways that happens -- long or int width, arithmetic or comparison, and for
 * the narrow types whether it can stay in A -- follows from the types of the
 * two values, which only the generator can see, so it decides.
 *
 * `narrow` is the one-byte or two-byte type the result is on its way into,
 * and 0 when there is no such destination or the operator is not one that may
 * truncate as it goes. It is the parser's to answer because it is a fact
 * about the text after the operator, not about the values.
 */
void vapply(unsigned char op, Type narrow);
void vaddr_local(int offset, Type type);  /* &local */
void vderef(void);                    /* *p, replacing the pointer */
void vmember(int offset, Type type, int ext, int quals);  /* p->m, from p */
void vstore_indirect(void);           /* *p = v, with p under v */
void vneg(void);
void vnot(void);
int  vpop_reg(void);                  /* force the top into a register */
void vdrop(void);
int  gen_inline_begin(int size, int *lock);  /* room for inlined parameters */
void gen_inline_end(int lock);
void gen_discard(void);               /* drop a value nothing will read */
void gen_stmt_end(void);              /* the scratch area is free again */
void gen_value_end(void);             /* all of it nothing still holds */

void gen_call(int fn, int nargs, int params_first, int nparams);
int         gen_nwants(void);       /* the names an object wants: gen_want */
const char *gen_want_name(int i);
void gen_call_indirect(int nargs);      /* through the pointer under them */
void vpush_function(int fn);          /* a function's address */
void vpush_global_addr(int sym);      /* a variable's, when it has none yet */
void gen_data_fixup(int fn, int at);  /* and one in a global's bytes */
void gen_link_fixup(int fn, int at);  /* and one in a link's */
extern int gen_data_context;          /* a global's initial value is being read */
extern int gen_pending_sym;            /* whose address it needs, not yet known */

/* The parser's changes to those two. In opt-acc they are calls, which
 * genlog.c logs, since what vpush_global_addr does depends on them; in acc
 * they are the assignments themselves. */
#ifdef OPT_ACC
void gen_data_begin(void);            /* func.c */
void gen_data_end(void);
void gen_pending_clear(void);
#else
#define gen_data_begin()    (gen_pending_sym = SYM_NONE, gen_data_context = 1)
#define gen_data_end()      (gen_data_context = 0)
#define gen_pending_clear() (gen_pending_sym = SYM_NONE)
#endif

/* A cast: the top converted to `to`, as an assignment to an object of that
 * type would convert it, or thrown away for `(void)`. */
void vcast(Type to, int ext, int quals);

/* A point the output can be taken back to, and everything the generator
 * knows about what it emitted since: sizeof has to parse its operand to
 * learn its type, and C says the operand is not evaluated, so the code it
 * produced -- and any call or array address waiting to be patched in it --
 * is undone afterwards. The values below the operand are put back as they
 * were too, since making room for it may have moved them out of registers
 * by code that no longer exists. */
typedef struct {
    int    at, nfixups, nrt_fixups, nbss_fixups, narray_patches, spill_used;
    int    spill_locked, vtop;
    int    nwide_consts;
    Value *saved;
} GenMark;

void gen_mark(GenMark *m);
void gen_rollback(GenMark *m);

/* Branches. A jump whose target is not known yet is emitted with a hole and
 * filled in by gen_label once the target is reached; one going backwards is
 * emitted with the address it already has. */
int  gen_here(void);                  /* the address a backward jump aims at */
int  gen_jump(void);                  /* jp nn, to be patched; returns the hole */
void gen_jump_to(int target);         /* jp nn, backwards */
int  gen_jump_if_false(void);         /* pop the top, jump when it is zero */
void gen_jump_if_true_to(int target); /* pop the top, jump back when it is not */

/* A switch: the value it switches on, stored in a frame slot when the switch
 * begins, loaded once where the cases are tested, and compared with each
 * case's constant in turn. */
void gen_switch_load(int slot, Type type);
void gen_switch_case(long value, uint32_t high, Type type, int target,
                     int slot);
int  gen_logic_left(int settles);     /* && and ||: after the left operand */
void gen_logic_right(int settles, int early);  /* and after the right */
void vtruth(int op);                  /* compare the top with zero: TK_NE, TK_EQ */
void vdup(void);                      /* the top twice */
void vswap(void);                     /* the top two the other way round */
void vprefix_local(int offset, Type type, int ext, int op);   /* ++x, --x */
void vpostfix_local(int offset, Type type, int ext, int op);  /* x++, x-- */
void vprefix_indirect(int op);        /* ++*p, with p on the stack */
void vpostfix_indirect(int op);       /* (*p)++, with p on the stack */
int  gen_cond_begin(int *slot, int *lock);    /* ?: after the condition */
int  gen_cond_middle(int *slot, Type *middle, int *middle_ext, int *middle_null);
void gen_cond_end(int to_stub, int slot, int lock, Type middle,
                  int middle_ext, int middle_null);
void gen_label(int hole);             /* fill a hole in with here */
void gen_cond_same(Type middle, int middle_ext);  /* the two sides agree */
int  gen_cond_middle_void(void);      /* `c ? f() : g()`, both of them void */
void gen_cond_end_void(int to_stub, int lock);
void gen_return(int line, const char *spot); /* `return`, where it is */
/* What a program has of the Agon's memory: the image, what it leaves at
 * zero, and then its heap and its stack. */
#define ACC_RAM_BYTES 458752

/* Kept between the heap's top and the top of memory, for the stack to come
 * down into. The same figure src/agon.ld reserves for acc's own stack, for
 * the same reason and with as little to go on. */
#define ACC_STACK_RESERVE 16384

/* Variables that start at zero and so take no room in the file. */
int  gen_bss_reserve(int bytes);      /* room in it; returns where */
int  gen_bss_reserve_aligned(int bytes, int align);  /* 2^align apart */
void gen_late_fixup(int sym, int at, int kind, long addend);  /* sym -1: bss */
int  gen_nlate(void);                  /* and the symbols they wait on */
int  gen_late_sym(int i);
void gen_slot(int at, int kind, long value);    /* a slot of a kind, filled */
void gen_bss_symbol(int sym, int at); /* and which symbol is there */
int  gen_bss_offset(int sym);         /* where one is, or -1 */
void gen_bss_move(int at, int bytes, int to);  /* its uses, into the image */
void gen_bss_forget(int sym);         /* it is not in the bss after all */
void gen_bss_fixup(int at);           /* a slot that wants the bss's start */
int  gen_nbss_fixups(void);
int  gen_bss_fixup_at(int i);
int  gen_bss_len(void);

void gen_finish(void);          /* resolve calls to functions defined later */
void gen_settle(int sym);       /* fill the uses of a symbol just given room */

/* What is still waiting on something, which is what a link takes to a
 * library to ask whether it has it. */
int gen_nfixups(void);
int gen_fixup_sym(int i);
int gen_no_address(int sym);

/* -c: what this file could not resolve, for the object to hand on. */
extern int gen_objects;
int gen_nexterns(void);
void rt_name_all(void);                /* the runtime, named for the link */
int  gen_rt_first(const int **at, const int **sym); /* see runtime.c */
int  gen_fixup_at(int i);
int gen_externs_helpers(void);        /* the first so many are helpers' */
int gen_extern_at(int i);
int gen_extern_sym(int i);
/* What the entry stub does with main's result: returns it to MOS, as
 * agondev's does; prints it as six hex digits first (-p); or reports its low
 * byte to IO port 0, which stops the emulator (-x). */
enum { END_RETURN, END_PRINT, END_EXIT };
void gen_startup(int ending, const char *program); /* the entry stub */

#endif
