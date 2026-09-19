/*
 * acc -- a C compiler for the Agon Light.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
#include <stdint.h>
#include <string.h>

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

/* A pointer is never a floating type, however floating the thing it points
 * at. Written as one mask and one compare rather than as "not a pointer and
 * floating", because the two-part form makes the compiler normalise both
 * sides of every `type_float(a) != type_float(b)` to 0 or 1, and on this
 * target that is a shift, and a shift is a call. The depth bits sit directly
 * above the floating bit, so the two together are equal to TY_FLOATING alone
 * exactly when the depth is zero and the bit is set. */
#define type_float(ty)    (((ty) & (TY_PTR_MASK | TY_FLOATING)) == TY_FLOATING)
#define TY_VOID     ((Type) 0)

/* How many times a type is a pointer, in the top three bits.
 *
 * A type is one byte and has to stay one byte: a Sym is eight, which is what
 * makes indexing the symbol table a shift rather than a multiply, and a
 * ninth byte would round it up to twelve. So the pointer depth shares the
 * byte with what it points at -- `int *` is a depth of one over TY_INT, and
 * `char **` a depth of two over TY_CHAR -- and the width of the pointer
 * itself is not stored at all, because every pointer on this machine is the
 * same three bytes an int is.
 *
 * Seven levels. Nobody writing C for a machine with 512 KB of address space
 * is going to miss the eighth.
 */
#define TY_PTR_SHIFT  5
#define TY_PTR_MASK   0xe0
#define TY_PTR_MAX    7

#define type_pointer(ty)   ((ty) & TY_PTR_MASK)
#define type_ptr_to(ty)    ((Type) ((ty) + (1 << TY_PTR_SHIFT)))
#define type_deref(ty)     ((Type) ((ty) - (1 << TY_PTR_SHIFT)))
#define type_ptr_depth(ty) (((ty) & TY_PTR_MASK) >> TY_PTR_SHIFT)

/* A long is wider than any register, so it never lives in one: it stays in
 * the frame and the code generator works on it there. type_wide says which
 * values that applies to. A pointer never does, however wide the thing it
 * points at. */
#define type_wide(ty)     (type_size(ty) > ACC_INT_SIZE)

#define type_size(ty)     (type_pointer(ty) ? ACC_INT_SIZE \
                                            : (int) ((ty) & TY_SIZE_MASK))
/* An address has no sign, so a pointer is unsigned -- which is one mask,
 * for the same reason. */
#define type_unsigned(ty) ((ty) & (TY_PTR_MASK | TY_UNSIGNED))

/* What `p + 1` moves by, which is the width of what p points at. */
#define type_step(ty)     type_size(type_deref(ty))

/* C promotes anything narrower than int to int before doing arithmetic on it,
 * so a value in a register is always int-wide. Only loads, stores and casts
 * deal in the narrow widths. */
#define type_promote(ty)  ((Type) (type_size(ty) < ACC_INT_SIZE ? TY_INT : (ty)))

/* Two pointers are the same type when they agree all the way down. Compared
 * whole rather than piecewise: the encoding puts the depth and what it points
 * at in the one byte, so equality of the byte is equality of the type. */
#define type_same(a, b)   ((a) == (b))

/* ------------------------------------------------------------------ */
/* diagnostics                                                         */

/* Both report and do not return, and are declared so: a caller then has
 * nothing to keep for after the call, which in the lexer was a stack frame on
 * every token to hold values only an error path would have gone on to use. */
__attribute__((noreturn)) void acc_error(const char *fmt, ...);
__attribute__((noreturn)) void acc_error_at(int line, const char *fmt, ...);

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

extern char *name_arena;    /* for name_global, further down */

/* The floating literal at s, as the bits of the nearest float; returns where
 * it ends, which is s if it has no digits. In float.c. */
const char *float_literal(const char *s, uint32_t *bits);
uint32_t    float_from_int(uint32_t magnitude, int negative);

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
     * three does is one compare after every local the program reads. */
    TK_INC, TK_DEC, TK_LBRACKET,
    TK_QUESTION, TK_COLON,

    /* Tokens added since, at the end: inserting one earlier renumbers every
     * token after it, which changes how clang lowers the switches and tables
     * keyed on them, and adding `for` among the keywords made the compiler
     * 1.5% slower on programs that do not use it. */
    TK_KW_FOR,
    TK_RBRACKET,

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
                                     && TK_OROR == TK_ANDAND + 1) ? 1 : -1];

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
typedef char tokens_fit_a_byte[TK_COUNT <= 256 ? 1 : -1];
typedef char postfix_tokens_are_adjacent[(TK_DEC == TK_INC + 1
                                          && TK_LBRACKET == TK_INC + 2) ? 1 : -1];
void expect_failed(const char *what);

/* Step past the current token, which has to be `token`. A macro for the same
 * reason accept is one, the other way round: nearly always the token is there,
 * and the call to find that out opened a frame to compare two bytes and then
 * call next. `token` is evaluated once and `what` only on the way to an
 * error. */
#define expect(token, what) \
    ((void) (tok == (token) ? next() : expect_failed(what)))
const char *tok_spelling(int token);

/* ------------------------------------------------------------------ */
/* symbols                                                             */

/* Arrays have the type of an element. A local one is not at a frame offset
 * known when it is declared -- see gen_local_array -- so its val is its
 * number in the function's array area; a global one's is its address.
 *
 * The two kinds that belong to a function come first, so that telling a
 * local from a file-scope symbol is one compare. */
enum {
    SYM_LOCAL,          /* a local or a parameter: val is its frame offset */
    SYM_LOCAL_ARRAY,    /* a local array: val is its number */
    SYM_FUNC,           /* a function: val is its address in the image */
    SYM_GLOBAL,         /* a file-scope variable: val is its address */
    SYM_GLOBAL_ARRAY    /* a file-scope array: val is its address */
};

/* Whether a symbol of this kind belongs to the function being compiled
 * rather than to file scope. */
#define sym_kind_local(kind) ((unsigned) (kind) <= SYM_LOCAL_ARRAY)

/* Eight bytes on the target: a name, a kind, one number whose meaning the
 * kind decides, and a type. tinycc's equivalent is 31, and at four hundred
 * lines of input that difference was 40 KB of a 206 KB budget. */
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
/* Symbol i. Good until the next sym_push, which may move the table; nothing
 * holds one across a push. A macro over the table itself rather than a call,
 * because it is used on every name the program mentions and the call opened a
 * frame to do one add.
 *
 * An index is the symbol's offset into the table in bytes, which is what makes
 * it one add: see sym.c. */
extern Sym *sym_table;
#define sym_at(i)  ((Sym *) ((char *) sym_table + (i)))
void sym_drop_locals(void);              /* at the end of a function */

/* A scope inside a function, which so far only a `for` has: the locals it
 * declares are dropped at its end, and their frame bytes are not reused. */
int  sym_scope_begin(void);
void sym_scope_end(int mark);

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
int  gen_local_array(void);           /* a new local array; returns its number */
void gen_local_array_size(int array, int size);  /* and how big, once known */
void vaddr_array(int array, Type elem);  /* its first element's address */
void gen_zero_array(int array, int from, int size);  /* zero part of one */

void vpush_const(int val, Type type);
void vpush_const_long(long val, Type type);  /* four bytes, so it goes to the frame */
void vpush_const_float(float val);
void vconvert(Type to);               /* narrow the top, then widen it back */
Type vtype(void);                     /* the type of the top */
int  vconst_top(int *val, Type *type);  /* whether the top is a constant */
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
void vaddr_local(int offset, Type type);  /* &local */
void vderef(void);                    /* *p, replacing the pointer */
void vstore_indirect(void);           /* *p = v, with p under v */
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
int  gen_logic_left(int settles);     /* && and ||: after the left operand */
void gen_logic_right(int settles, int early);  /* and after the right */
void vtruth(int op);                  /* compare the top with zero: TK_NE, TK_EQ */
void vdup(void);                      /* the top twice */
void vprefix_local(int offset, Type type, int op);   /* ++x, --x */
void vpostfix_local(int offset, Type type, int op);  /* x++, x-- */
void vprefix_indirect(int op);        /* ++*p, with p on the stack */
void vpostfix_indirect(int op);       /* (*p)++, with p on the stack */
int  gen_cond_begin(int *slot);       /* ?: after the condition */
int  gen_cond_middle(int slot, Type *middle, int *middle_null);
void gen_cond_end(int to_stub, int slot, Type middle, int middle_null);
void gen_label(int hole);             /* fill a hole in with here */
void gen_return(void);
void gen_finish(void);          /* resolve calls to functions defined later */
void gen_startup(int report_by_exit);  /* the entry stub MOS lands on */

/* ------------------------------------------------------------------ */
/* output                                                              */

void out_open(const char *path);
void out_close(void);
/* The output's write position and its end, and the growth that happens a
 * dozen times a compile. Visible so that the three emitters below can be
 * inlined: every byte the compiler emits goes through one of them, and as
 * calls each opened a frame on this target to do a compare and a store. */
extern unsigned char *out_put, *out_limit;
void out_grow(void);

/* The three bytes of a 24-bit value, lowest first, at `at`.
 *
 * Copied rather than shifted out. `value >> 8` and `value >> 16` are each a
 * call into the runtime on this target -- there is no instruction that shifts
 * a 24-bit register by more than one place -- and this runs for every call,
 * jump and constant the compiler emits. The value is already those three
 * bytes, lowest first, in memory, so copying them is the whole job. On a host
 * that keeps its bytes the other way round it is not, and the shifts are used
 * there instead. */
static inline __attribute__((always_inline))
void put24(unsigned char *at, int value)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    at[0] = (unsigned char) value;
    at[1] = (unsigned char) (value >> 8);
    at[2] = (unsigned char) (value >> 16);
#else
    memcpy(at, &value, 3);
#endif
}

/* The 24-bit value put24 stored at `at`, for the same reason and in the same
 * way: one load, where assembling it from bytes would be shifts. Values are
 * non-negative. */
static inline __attribute__((always_inline))
int get24(const unsigned char *at)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return at[0] | (at[1] << 8) | (at[2] << 16);
#else
    int value = 0;

    memcpy(&value, at, 3);

    return value;
#endif
}

/* A name's file-scope symbol lives in the three bytes in front of its text in
 * the arena, stored plus one so that the zero a new name starts with means
 * none. Finding a function is then one load, where it used to be a hash
 * table keyed on the NameRef: a mask and a shift to find the slot, and a
 * probe, for every name the program mentions that is not a local. A NameRef
 * is already unique per name, which is what a hash would have been for.
 *
 * A name has at most one file-scope symbol -- every push of one goes through
 * a sym_find that came back empty -- which is what lets one field hold it. */
static inline __attribute__((always_inline))
int name_global(NameRef ref)
{
    return get24((const unsigned char *) name_arena + ref - 3) - 1;
}

static inline __attribute__((always_inline))
void name_set_global(NameRef ref, int sym)
{
    put24((unsigned char *) name_arena + ref - 3, sym + 1);
}

/* An opcode and the 24-bit operand that follows it, which is the shape of a
 * call, a jump and every load of a constant. Four bytes, one bounds check. */
static inline __attribute__((always_inline))
void out_opcode24(int opcode, int value)
{
    if (out_limit - out_put < 4)
        out_grow();
    out_put[0] = (unsigned char) opcode;
    put24(out_put + 1, value);
    out_put += 4;
}

static inline __attribute__((always_inline)) void out_byte(int byte)
{
    if (out_put == out_limit)
        out_grow();
    *out_put++ = (unsigned char) byte;
}

static inline __attribute__((always_inline)) void out_byte2(int first, int second)
{
    if (out_limit - out_put < 2)
        out_grow();
    out_put[0] = (unsigned char) first;
    out_put[1] = (unsigned char) second;
    out_put += 2;
}

static inline __attribute__((always_inline)) void out_byte3(int first, int second, int third)
{
    if (out_limit - out_put < 3)
        out_grow();
    out_put[0] = (unsigned char) first;
    out_put[1] = (unsigned char) second;
    out_put[2] = (unsigned char) third;
    out_put += 3;
}
void out_word24(int v);
int  out_here(void);                  /* the address the next byte will have */
void out_patch24(int at, int v);

#endif /* ACC_H */
