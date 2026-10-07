/*
 * Tokens and the preprocessor: what the lexer hands the parser, and the
 * calls that drive it. See source.c, macro.c, directive.c and lex.c.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_LEX_H
#define ACC_LEX_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "names.h"

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

    /* Every other word C99 reserves. acc implements none of them, and they
     * are all one token because there is nothing to tell them apart for:
     * what the parser does with any of them is name it and stop. Without
     * this they lex as ordinary identifiers and the diagnostic comes out as
     * "'break' is not declared", which points at the wrong thing entirely. */
    TK_KW_RESERVED,
    TK_FLOAT,        /* a floating literal; tok_fval holds it */

    /* punctuation, one per spelling so the parser never re-reads text */
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_SEMI, TK_COMMA, TK_ASSIGN,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_AMP, TK_PIPE, TK_CARET, TK_TILDE,
    TK_SHL, TK_SHR,
    TK_LT, TK_GT, TK_LE, TK_GE, TK_EQ, TK_NE,
    TK_NOT,
    TK_ANDAND, TK_OROR,

    /* x op= y, one token apiece. */
    TK_ADD_ASSIGN, TK_SUB_ASSIGN, TK_MUL_ASSIGN, TK_DIV_ASSIGN, TK_MOD_ASSIGN,
    TK_AND_ASSIGN, TK_OR_ASSIGN, TK_XOR_ASSIGN, TK_SHL_ASSIGN, TK_SHR_ASSIGN,

    /* What can follow a name and act on it, together: whether any of the
     * five does is one compare after every local the program reads. */
    TK_INC, TK_DEC, TK_LBRACKET, TK_DOT, TK_ARROW,
    TK_QUESTION, TK_COLON,

    /* Tokens added since, at the end: inserting one earlier renumbers every
     * token after it, which changes how clang lowers the switches and tables
     * keyed on them, and adding `for` among the keywords made the compiler
     * 1.5% slower on programs that do not use it. */
    TK_KW_FOR,
    TK_RBRACKET,
    TK_KW_BREAK, TK_KW_CONTINUE, TK_KW_DO, TK_KW_SWITCH, TK_KW_CASE,
    TK_KW_DEFAULT,
    TK_KW_GOTO,
    TK_STRING,                  /* a string literal; tok_str holds its bytes */
    TK_KW_SIZEOF,
    TK_KW_ENUM, TK_KW_STRUCT, TK_KW_UNION, TK_KW_TYPEDEF,
    TK_KW_STATIC, TK_KW_EXTERN, TK_KW_AUTO, TK_KW_REGISTER,
    TK_KW_CONST, TK_KW_VOLATILE, TK_KW_RESTRICT,
    TK_KW_INLINE, TK_KW_BOOL,
    TK_ELLIPSIS,
    TK_KW_VA_LIST, TK_KW_VA_START, TK_KW_VA_ARG, TK_KW_VA_END, TK_KW_VA_COPY,
    TK_KW_STATIC_ASSERT, TK_KW_OFFSETOF,
    TK_KW_ATTRIBUTE,

    /* The names that stand for something the lexer knows and the program
     * does not write: the file it is in, the line it is on, when the
     * compile happened, and that this is a C compiler at all. They are
     * interned with the keywords, which is what makes them free -- a
     * keyword is already a name whose token code sits in the arena, so
     * recognising one costs nothing that was not being paid. Last in the
     * enum, and so the highest codes there are, so that telling them from
     * every other keyword is one compare. */
    TK_FILE, TK_LINE, TK_DATE, TK_TIME, TK_STDC,
    TK_STDC_VERSION, TK_STDC_HOSTED,

    /* And one that is not a macro but is recognised the same way, since the
     * lexer sees it only after it is interned and a code here costs the
     * names that are not it nothing: _Pragma, the operator form of
     * #pragma. (L, which begins a wide literal, cannot be one of these: a
     * program may declare a file-scope L, and a keyword's code is kept
     * where a file-scope name's symbol is. See NAME_WIDE.) */
    TK_PRAGMA_OP,

    TK_COUNT                     /* how many there are, for tables keyed on one */
};

extern int      tok;        /* the current token */
/* Wide enough for a long, because C99 types a constant by the first type that
 * can hold it and acc now has one. It is read once per numeric token, so the
 * long arithmetic that costs on this target is not on a path that matters. */
/* Whether a token is `first` or the one after it in the enum.
 *
 * The obvious `t == A || t == A + 1` is what this means, and the compiler
 * merges it into `(t & ~1) == A` whenever A is even -- which is a sensible
 * thing to do on most machines, and on this one an AND of two 24-bit values
 * is a call into the runtime. Several of these sat on the path of every
 * operator in every program. Narrowing to a byte first and then subtracting
 * and comparing is three instructions and no call -- and the narrowing has to
 * come first: `(unsigned char) (t - first) < 2` is the same arithmetic, and
 * the compiler turned it back into the 24-bit AND it was meant to avoid. */
#define tok_pair(t, first)  ((unsigned char) (t) - (unsigned) (first) < 2u)

/* The pairs it is used on, which have to stay next to each other in the enum
 * or it quietly tests the wrong thing. An array of negative size will not
 * compile, which is C99's way of asserting at build time. */
typedef char tok_pairs_are_adjacent[(TK_SHR == TK_SHL + 1
                                     && TK_LE == TK_GT + 1
                                     && TK_NE == TK_EQ + 1
                                     && TK_COMMA == TK_SEMI + 1
                                     && TK_OROR == TK_ANDAND + 1
                                     && TK_PERCENT == TK_SLASH + 1) ? 1 : -1];

extern long     tok_val;    /* its value, when TK_INT */
extern uint32_t tok_val_hi; /* and its high half, when a long long */
extern const char *tok_str; /* its bytes, when TK_STRING, escapes undone */
extern int      tok_str_len; /* and how many, without a terminator */
extern int      tok_str_wide; /* whether they are wchar_t, two bytes each */
extern int      tok_str_escaped; /* a narrow one's escape gave a byte past 0x7f */
extern Type     tok_type;   /* and its type, which C99 fixes by its size */
extern float    tok_fval;   /* its value, when TK_FLOAT */
extern NameRef  tok_name;   /* its name, when TK_IDENT */
extern int      tok_line;   /* the line it started on */
extern int      tok_prev_line; /* where the token before it ended */
extern const char *tok_at;      /* where it starts, for its column */
int lex_col(void);              /* the column it starts at, 0 if not known */
int lex_col_at(const char *at); /* the column of a tok_at kept, or 0 */
int lex_prev_col(void);         /* the column just past the token before it */
int lex_name_col(const char *at, int *line); /* of the name before `at` */

void lex_init(void);
void lex_open(const char *path);
void lex_add_include(const char *dir);
void lex_add_include_default(const char *dir);
void name_table_free(void);         /* before an object is written */
void sym_members_free(void);        /* once the source is read */
void lex_define(const char *arg);       /* -D, as a #define would read it */
void lex_undefine(const char *arg);     /* -U, as a #undef would */

/* Every file the compile read, and enough about each to tell whether it has
 * changed: see the note in src/source.c. The last is the -D, -U and -I options,
 * with the empty path. */
void        lex_want_deps(void);
int         lex_ndeps(void);
const char *lex_dep_path(int i);
void        lex_dep_marks(int i, unsigned *size, unsigned *sum,
                          unsigned *weighted);
int         lex_file_marks(const char *path, unsigned *size, unsigned *sum,
                           unsigned *weighted);
void lex_close(void);
void lex_end(void);   /* what has to be finished before the file is */
void next(void);                 /* advance to the following token */
int  lex_colon_follows(void);    /* whether `:` comes after the current token */
int  lex_rparen_follows(void);   /* and whether `)` does */
int  lex_rbracket_follows(void); /* or `]` */
int  lex_string_follows(void);   /* or a string */
extern int lex_trigraphs;       /* -trigraphs: see src/source.c */
extern const char *lex_prelude; /* -include: read before the source */
extern char *lex_record;              /* a size's text, being kept */
void  lex_record_from(char *text, char *end);  /* from after this token */
char *lex_record_take(void);         /* to this `]`: its end, or NULL */
char *lex_record_take_semi(void);    /* to this `;`: its end, or NULL */
char *lex_record_take_paren(void);   /* to this `)`: its end, or NULL */
#ifdef OPT_ACC
void  lex_body_record_from(void);    /* a function's body, from after the `{` */
char *lex_body_record_take(void);    /* to this `}`: the text, or NULL */

/* always_inline and noinline, said of a function and not yet taken. */
enum { INLINE_ALWAYS = 1, INLINE_NEVER = 2 };
extern int attr_inline;
#endif

/* The current token, as lex_token_save sets it aside. */
typedef struct {
    int         tok, line, prev_line, str_len, str_wide, str_escaped;
    long        val;
    uint32_t    val_hi;
    float       fval;
    NameRef     name;
    Type        type;
    const char *str;
    const char *at;             /* where it starts, for its column */
} LexToken;

void lex_token_save(LexToken *t);
void lex_token_restore(const LexToken *t);
int  lex_macro_def(NameRef name);     /* which definition, or 0 */
void  lex_push_record(char *text, int len);    /* and read again */
void  lex_push_record_owned(char *text);   /* and freed when read */
void  lex_pop_record(void);               /* read to its end: closed */
int  lex_ident_follows(void);    /* and whether a name does */
int  lex_rbrace_follows(void);   /* and whether `}` does */
int         accept_next(void);          /* next(), returning 1, for accept */

/* The current token if it is `token`, stepping past it; 0 and nothing done if
 * not. A macro because most of the time the answer is no -- the parser tries
 * `,` or `=` or `(` after something that is followed by none of them -- and a
 * call to find that out opened a frame on this target before comparing two
 * bytes. `token` is evaluated once. */
#define accept(token)  (tok == (token) ? accept_next() : 0)

/* tok against one token by its low byte, which is all of it: every token is
 * below 256. For the tests on the hottest paths -- whether a `[` follows each
 * name the program reads -- where a 24-bit compare is seven instructions and
 * a byte compare two. */
#define tok_is(token)  ((unsigned char) tok == (token))

/* The current token's low byte, read as a byte. Narrowing tok with a cast is
 * not the same thing on this target: clang loads all three bytes and masks
 * them, and a mask of a 24-bit value is a call into the runtime -- which
 * `(unsigned char) tok - X < 3` made on every operand the program has. */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define tok_low  ((unsigned char) tok)
#else
#define tok_low  (*(const unsigned char *) &tok)
#endif
typedef char tokens_fit_a_byte[TK_COUNT <= 256 ? 1 : -1];
typedef char postfix_tokens_are_adjacent[(TK_DEC == TK_INC + 1
                                          && TK_LBRACKET == TK_INC + 2
                                          && TK_DOT == TK_INC + 3
                                          && TK_ARROW == TK_INC + 4) ? 1 : -1];
void expect_failed(const char *what);

/* Step past the current token, which has to be `token`. A macro for the same
 * reason accept is one, the other way round: nearly always the token is there,
 * and the call to find that out opened a frame to compare two bytes and then
 * call next. `token` is evaluated once and `what` only on the way to an
 * error. */
#define expect(token, what) \
    ((void) (tok == (token) ? next() : expect_failed(what)))
const char *tok_spelling(int token);

#endif
