/*
 * acc -- a C compiler for the Agon Light.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 *
 * One pass, no syntax tree, no intermediate representation. The parser emits
 * eZ80 machine code as it reads, with a small stack of *descriptions* of
 * values -- a constant, a local at a frame offset, something already in a
 * register -- that are only turned into instructions when something forces
 * them. That is tinycc's model and it is the right one: it is the fastest
 * way to compile C, and more to the point here it is the smallest.
 *
 * The language is C99. What is here is a subset of it, grown a feature at a
 * time with a test for each; every gap is something not written yet rather
 * than something ruled out, and the ones that are are named where the
 * compiler refuses them.
 *
 * What is not tinycc's is the memory. The Agon gives a program 448 KB for
 * code, data, heap and stack together, and measured on that machine tinycc
 * spends 31 bytes on every symbol and 75 KB on machinery for a preprocessor
 * this compiler does not have yet. Everything below is sized for the target
 * rather than for a workstation: symbols are 7 bytes, names live in one arena
 * that is never freed, and the output is written as it is produced.
 */
#ifndef ACC_H
#define ACC_H

#include <stddef.h>

/* The target's own widths, which are also the host's when acc is compiled by
 * agondev to run on the Agon. Named rather than assumed, because the whole
 * difficulty of the previous compiler was code that assumed 4. */
#define ACC_INT_SIZE   3
#define ACC_PTR_SIZE   3
#define ACC_LONG_SIZE  4

/* ------------------------------------------------------------------ */
/* types                                                               */

/* An integer type is its width in bytes and whether it is signed, and that is
 * everything the code generator needs: the width says how many bytes a load
 * or a store touches, and the sign says how a narrow value is widened and how
 * two values compare.
 *
 * The widths are agondev's, so that a program compiled by both comes out the
 * same: char 1, short 2, int and pointers 3. long is 4 and long long 8, and
 * neither is here yet -- both need a value to live in more than one register,
 * which is a different piece of work from this one. */
typedef unsigned char Type;

#define TY_SIZE_MASK  0x07              /* the width, in bytes */
#define TY_UNSIGNED   0x08

#define TY_CHAR     ((Type) 1)
#define TY_UCHAR    ((Type) (1 | TY_UNSIGNED))
#define TY_SHORT    ((Type) 2)
#define TY_USHORT   ((Type) (2 | TY_UNSIGNED))
#define TY_INT      ((Type) ACC_INT_SIZE)
#define TY_UINT     ((Type) (ACC_INT_SIZE | TY_UNSIGNED))
#define TY_LONG     ((Type) ACC_LONG_SIZE)
#define TY_ULONG    ((Type) (ACC_LONG_SIZE | TY_UNSIGNED))

/* float and double are one type here: agondev makes both four-byte IEEE 754
 * single precision, and compiles double arithmetic to the same routines. So
 * there is one floating type, four bytes wide, and the flag that marks it has
 * to live outside the width field -- which is why it is 0x10 rather than a
 * size the width could be confused with. */
#define TY_FLOATING 0x10
#define TY_FLOAT    ((Type) (ACC_LONG_SIZE | TY_FLOATING))

#define type_float(ty)    ((ty) & TY_FLOATING)
#define TY_VOID     ((Type) 0)

/* A long is wider than any register, so it never lives in one: it stays in
 * the frame and the code generator works on it there. type_wide says which
 * values that applies to. */
#define type_wide(ty)     (type_size(ty) > ACC_INT_SIZE)

#define type_size(ty)     ((int) ((ty) & TY_SIZE_MASK))
#define type_unsigned(ty) ((ty) & TY_UNSIGNED)

/* C promotes anything narrower than int to int before doing arithmetic on it,
 * so a value in a register is always int-wide. Only loads, stores and casts
 * deal in the narrow widths. */
#define type_promote(ty)  ((Type) (type_size(ty) < ACC_INT_SIZE ? TY_INT : (ty)))

/* ------------------------------------------------------------------ */
/* diagnostics                                                         */

void acc_error(const char *fmt, ...);   /* reports and does not return */
void acc_error_at(int line, const char *fmt, ...);

/* ------------------------------------------------------------------ */
/* names                                                               */

/* Every identifier in the program is stored once, in one arena that grows and
 * is never freed. A name is referred to by its offset into that arena, which
 * is three bytes rather than a pointer plus a length plus a hash link. */
typedef unsigned int NameRef;
#define NAME_NONE ((NameRef) 0)

NameRef     name_intern(const char *text, int len);
const char *name_text(NameRef ref);
void        name_init(void);

/* ------------------------------------------------------------------ */
/* tokens                                                              */

enum {
    TK_EOF = 0,
    TK_INT,          /* an integer literal; tok_val holds it */
    TK_IDENT,        /* an identifier; tok_name holds it */

    /* keywords, which are identifiers the lexer recognises */
    TK_KW_INT,
    TK_KW_VOID,
    TK_KW_RETURN,
    TK_KW_IF,
    TK_KW_ELSE,
    TK_KW_WHILE,
    TK_KW_CHAR,
    TK_KW_SHORT,
    TK_KW_LONG,
    TK_KW_SIGNED,
    TK_KW_UNSIGNED,
    TK_KW_FLOAT,
    TK_KW_DOUBLE,
    TK_FLOAT,        /* a floating literal; tok_fval holds it */

    /* punctuation, one per spelling so the parser never re-reads text */
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_SEMI, TK_COMMA, TK_ASSIGN,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_AMP, TK_PIPE, TK_CARET, TK_TILDE,
    TK_SHL, TK_SHR,
    TK_LT, TK_GT, TK_LE, TK_GE, TK_EQ, TK_NE,
    TK_NOT,

    TK_COUNT                     /* how many there are, for tables keyed on one */
};

extern int      tok;        /* the current token */
/* Wide enough for a long, because C99 types a constant by the first type that
 * can hold it and acc now has one. It is read once per numeric token, so the
 * long arithmetic that costs on this target is not on a path that matters. */
extern long     tok_val;    /* its value, when TK_INT */
extern Type     tok_type;   /* and its type, which C99 fixes by its size */
extern float    tok_fval;   /* its value, when TK_FLOAT */
extern NameRef  tok_name;   /* its name, when TK_IDENT */
extern int      tok_line;   /* the line it started on */
extern int      tok_prev_line; /* where the token before it ended */

void lex_init(void);
void lex_open(const char *path);
void lex_close(void);
void next(void);                 /* advance to the following token */
int  accept(int token);          /* consume it if present; say whether it was */
void expect(int token, const char *what);
const char *tok_spelling(int token);
int         tok_is_unimplemented_op(int token); /* for diagnostics */

/* ------------------------------------------------------------------ */
/* symbols                                                             */

enum {
    SYM_LOCAL,      /* a local or a parameter: val is its frame offset */
    SYM_FUNC        /* a function: val is its address in the image */
};

/* Seven bytes on the target: a name, a kind, and one number whose meaning the
 * kind decides. tinycc's equivalent is 31, and at four hundred lines of input
 * that difference was 40 KB of a 206 KB budget. */
/* Eight bytes and not seven. Turning an index into an address always costs a
 * helper call on this target -- there is no barrel shifter and no multiplier
 * wider than the 8-bit MLT -- and the only choice is which one: a 7-byte
 * element is `call __imulu`, an 8-byte element is `call __ishl`, which is the
 * cheaper of the two. sym_push alone does five of them. The pad byte costs
 * one byte per symbol in a program that has a few thousand. */
typedef struct {
    NameRef       name;
    unsigned char kind;
    int           val;
    Type          type;         /* fits in what was the pad byte */
} Sym;

/* A function's parameter types, kept beside the symbols. A call converts each
 * argument to the type the parameter was declared with, which matters because
 * a long takes two argument slots where everything else takes one. */
int  sym_params_begin(void);
void sym_param_add(Type type);
Type sym_param_type(int first, int index);
void sym_set_params(int sym, int first, int count);
int  sym_params_first(int sym);
int  sym_nparams(int sym);

void sym_init(void);
/* Symbols are referred to by index. A Sym * is only good until the next push,
 * because the table is grown with realloc; see sym.c. */
#define SYM_NONE (-1)

int  sym_find(NameRef name);             /* innermost first; SYM_NONE if unknown */
int  sym_push(NameRef name, int kind, int val);
Sym *sym_at(int i);                      /* valid until the next sym_push */
void sym_drop_locals(void);              /* at the end of a function */

/* ------------------------------------------------------------------ */
/* values                                                              */

/* What the expression compiler pushes instead of emitting. A value is not
 * turned into instructions until something needs it in a register, so
 * `1 + 2` never reaches the code generator and `x + 1` loads x once. */
enum {
    VAL_CONST,      /* a literal; val holds it */
    VAL_LOCAL,      /* a local at frame offset val */
    VAL_REG,        /* already in register val */
    VAL_ACC         /* in A, still narrow: see the note on byte arithmetic */
};

/* What the parser knows about a value it has not had to emit yet: a constant,
 * a local at a frame offset, or something already in a register. `val` is the
 * constant, the offset or the register number, according to `kind`. */
typedef struct {
    unsigned char kind;
    Type          type;
    int           val;
    unsigned char pad[3];       /* eight bytes: see the note on Sym */
} Value;

/* ------------------------------------------------------------------ */
/* code generation                                                     */

/* The three 24-bit general registers in ADL mode. IX is the frame pointer and
 * IY is kept free as the backend's own scratch, which is what lets a value be
 * dereferenced without disturbing anything the allocator is holding. */
enum { R_HL = 0, R_DE, R_BC, NREGS };

void gen_init(void);
void gen_func_begin(int fn, int nparams, Type returns);
void gen_func_end(void);
int  gen_local(int size);             /* reserve a slot; returns its offset */

void vpush_const(int val, Type type);
void vpush_const_long(long val, Type type);  /* four bytes, so it goes to the frame */
void vpush_const_float(float val);
void vconvert(Type to);               /* narrow the top, then widen it back */
Type vtype(void);                     /* the type of the top */
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
void vapply(int op, Type narrow);
void vneg(void);
void vnot(void);
int  vpop_reg(void);                  /* force the top into a register */
void vdrop(void);
void gen_stmt_end(void);              /* the scratch area is free again */

void gen_call(int fn, int nargs, int params_first, int nparams);

/* Branches. A jump whose target is not known yet is emitted with a hole and
 * filled in by gen_label once the target is reached; one going backwards is
 * emitted with the address it already has. */
int  gen_here(void);                  /* the address a backward jump aims at */
int  gen_jump(void);                  /* jp nn, to be patched; returns the hole */
void gen_jump_to(int target);         /* jp nn, backwards */
int  gen_jump_if_false(void);         /* pop the top, jump when it is zero */
void gen_label(int hole);             /* fill a hole in with here */
void gen_return(void);
void gen_finish(void);          /* resolve calls to functions defined later */
void gen_startup(int report_by_exit);  /* the entry stub MOS lands on */

/* ------------------------------------------------------------------ */
/* output                                                              */

void out_open(const char *path);
void out_close(void);
void out_byte(int b);
void out_byte2(int first, int second);
void out_byte3(int first, int second, int third);
void out_opcode24(int opcode, int value);
void out_word24(int v);
int  out_here(void);                  /* the address the next byte will have */
void out_patch24(int at, int v);

#endif /* ACC_H */
