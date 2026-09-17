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

NameRef     name_intern(const char *s, int len);
const char *name_text(NameRef n);
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

    /* punctuation, one per spelling so the parser never re-reads text */
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_SEMI, TK_COMMA, TK_ASSIGN,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_AMP, TK_PIPE, TK_CARET, TK_TILDE,
    TK_SHL, TK_SHR
};

extern int      tok;        /* the current token */
extern int      tok_val;    /* its value, when TK_INT */
extern NameRef  tok_name;   /* its name, when TK_IDENT */
extern int      tok_line;   /* the line it started on */

void lex_init(void);
void lex_open(const char *path);
void lex_close(void);
void next(void);                 /* advance to the following token */
int  accept(int t);              /* consume t if present; say whether it was */
void expect(int t, const char *what);
const char *tok_spelling(int t); /* for diagnostics */

/* ------------------------------------------------------------------ */
/* symbols                                                             */

enum {
    SYM_LOCAL,      /* a local or a parameter: val is its frame offset */
    SYM_FUNC        /* a function: val is its address in the image */
};

/* Seven bytes on the target: a name, a kind, and one number whose meaning the
 * kind decides. tinycc's equivalent is 31, and at four hundred lines of input
 * that difference was 40 KB of a 206 KB budget. */
typedef struct {
    NameRef       name;
    unsigned char kind;
    int           val;
} Sym;

void sym_init(void);
Sym *sym_find(NameRef name);             /* innermost first; NULL if unknown */
Sym *sym_push(NameRef name, int kind, int val);
int  sym_mark(void);                     /* remember the current scope depth */
void sym_release(int mark);              /* drop everything pushed since */

/* ------------------------------------------------------------------ */
/* values                                                              */

/* What the expression compiler pushes instead of emitting. A value is not
 * turned into instructions until something needs it in a register, so
 * `1 + 2` never reaches the code generator and `x + 1` loads x once. */
enum {
    VAL_CONST,      /* a literal; v holds it */
    VAL_LOCAL,      /* a local at frame offset v */
    VAL_REG         /* already in register v */
};

typedef struct {
    unsigned char kind;
    int           v;
} Value;

/* ------------------------------------------------------------------ */
/* code generation                                                     */

/* The three 24-bit general registers in ADL mode. IX is the frame pointer and
 * IY is kept free as the backend's own scratch, which is what lets a value be
 * dereferenced without disturbing anything the allocator is holding. */
enum { R_HL = 0, R_DE, R_BC, NREGS };

void gen_init(void);
void gen_func_begin(Sym *fn, int nparams);
void gen_func_end(void);
int  gen_local(void);                 /* reserve a slot; returns its offset */

void vpush_const(int v);
void vpush_local(int offset);
void vpush_reg(int reg);
void vstore_local(int offset);        /* pop the top into a local */
void vbinop(int t);                   /* combine the top two with token t */
void vneg(void);
void vnot(void);
int  vpop_reg(void);                  /* force the top into a register */
void vdrop(void);

void gen_call(Sym *fn, int nargs);
void gen_return(void);
void gen_finish(void);          /* resolve calls to functions defined later */
void gen_startup(void);         /* the entry stub MOS lands on */

/* ------------------------------------------------------------------ */
/* output                                                              */

void out_open(const char *path);
void out_close(void);
void out_byte(int b);
void out_word24(int v);
int  out_here(void);                  /* the address the next byte will have */
void out_patch24(int at, int v);

#endif /* ACC_H */
