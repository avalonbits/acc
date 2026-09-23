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

/* Every identifier in the program is stored once, in one arena that grows and
 * is never freed. A name is referred to by its offset into that arena, which
 * is three bytes rather than a pointer plus a length plus a hash link. */
typedef unsigned int NameRef;
#define NAME_NONE ((NameRef) 0)

#define TY_SIZE_MASK  0x07              /* the width, in bytes */
#define TY_UNSIGNED   0x08

#define TY_CHAR     ((Type) 1)
#define TY_UCHAR    ((Type) (1 | TY_UNSIGNED))
#define TY_SHORT    ((Type) 2)
#define TY_USHORT   ((Type) (2 | TY_UNSIGNED))

/* _Bool: one byte, unsigned, read and stored as unsigned char is -- and
 * converting anything to it gives 0 or 1, which is the one thing about it
 * that is its own. The floating bit makes it a code no other type has. */
#define TY_BOOL     ((Type) (TY_FLOATING | TY_UNSIGNED | 1))
#define TY_INT      ((Type) ACC_INT_SIZE)
#define TY_UINT     ((Type) (ACC_INT_SIZE | TY_UNSIGNED))
#define TY_LONG     ((Type) ACC_LONG_SIZE)
#define TY_ULONG    ((Type) (ACC_LONG_SIZE | TY_UNSIGNED))

/* long long: eight bytes, which the three bits of the width cannot say. So
 * it has a width no other type has, 6, and type_bytes turns that into the
 * eight it stores as. type_size is left alone: it is on every path for every
 * other type, and a long long never needs its answer except where it is
 * stored, which is what type_bytes and type_wide_bytes are for. */
#define TY_LLONG    ((Type) 6)
#define TY_ULLONG   ((Type) (6 | TY_UNSIGNED))
#define type_eight(ty)     (type_size(ty) == 6)

/* float and double are one type here: agondev makes both four-byte IEEE 754
 * single precision, and compiles double arithmetic to the same routines. So
 * there is one floating type, four bytes wide, and the flag that marks it has
 * to live outside the width field -- which is why it is 0x10 rather than a
 * size the width could be confused with. */
#define TY_FLOATING 0x10
#define TY_FLOAT    ((Type) (ACC_LONG_SIZE | TY_FLOATING))

/* A pointer is never a floating type, however floating the thing it points
 * at, and an array type is never one either, although some of the codes
 * arrays are given have the floating bit set. So it is the whole byte that is
 * compared: float is one type, and nothing else is equal to it. One compare,
 * as the mask it replaces was. */
#define type_float(ty)    ((Type) (ty) == TY_FLOAT)
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

/* The bytes a wide value takes in the frame: four, or eight for a long long
 * long. Only ever asked of a type that is already known to be
 * wide, so it reads the width field directly rather than going through
 * type_size, whose answer for a pointer cannot arise here. */
#define type_wide_bytes(ty) (((ty) & TY_SIZE_MASK) == 6 ? 8 : ACC_LONG_SIZE)

#define type_size(ty)     (type_pointer(ty) ? ACC_INT_SIZE \
                                            : (int) ((ty) & TY_SIZE_MASK))
/* An address has no sign, so a pointer is unsigned -- which is one mask,
 * for the same reason. */
#define type_unsigned(ty) ((ty) & (TY_PTR_MASK | TY_UNSIGNED))

/* Extended types: arrays as types, and later structs.
 *
 * A type is one byte, and has to stay one: every value the compiler handles
 * carries one, and the tests on it -- its width, its sign, whether it is a
 * pointer -- run constantly. Widening it to 16 bits measured 1.5-4% slower
 * on every program, and to 24 bits 2.5-5.4%, whether or not the program had
 * anything a byte could not say.
 *
 * So the byte stays, and one code in it, TY_EXT, means "look at the other
 * byte": an extension, carried beside the type in a Sym and a Value, that
 * indexes a table of what the byte cannot say. The hot code only ever sees
 * the first byte, which is why this costs nothing where it is not used. The
 * pointer depth still sits on top, so a pointer to a row is a pointer like
 * any other, and a value's extension always describes the TY_EXT at the
 * bottom of its chain of pointers: taking an address or reading through one
 * changes the depth and leaves the extension alone.
 *
 * An array is not a type a value ever has -- it becomes the address of its
 * first element wherever it is used -- so a one-dimensional array needs no
 * extension: its symbol records the element type. What needs one is an
 * array of arrays, whose elements are rows, and a pointer to a whole array,
 * which steps by one. The table has 255 entries. */
#define TY_EXT      ((Type) 7)          /* a width no scalar has */

#define type_is_array(ty)   ((Type) (ty) == TY_EXT)

/* A struct or a union, whose extension says which and what is in it.
 *
 * A value of one is never loaded: it is carried as its address, which is
 * what an assignment copies from, a member is found from, and an argument is
 * copied out of. So it has the width of an address, which keeps it off every
 * path for four-byte values, and the floating bit, which no int has and
 * which float, being four bytes, is not confused with. Wherever a value is
 * loaded as the number it is, a struct is refused: see struct_used. */
#define TY_STRUCT   ((Type) (TY_FLOATING | ACC_INT_SIZE))

#define type_is_struct(ty)  ((Type) (ty) == TY_STRUCT)

/* A function, as a type: what a function's name is before it becomes the
 * address it always becomes, and what a pointer to a function points at.
 * Its extension says what it returns and takes. No value ever has it: a
 * function's name is its address, a pointer, straight away. */
#define TY_FUNC     ((Type) (TY_FLOATING | 5))
#define type_is_func(ty)    ((Type) (ty) == TY_FUNC)

/* The size of any type, arrays and structs included, given its extension. */
#define type_bytes(ty, x)   (type_is_array(ty) || type_is_struct(ty) \
                             ? ext_bytes(x) : type_scalar_bytes(ty))
#define type_scalar_bytes(ty) (type_eight(ty) ? 8 : type_size(ty))

/* What `p + 1` moves by, which is the width of what p points at -- a whole
 * row, when that is an array. */
#define type_step(ty, x)    type_bytes(type_deref(ty), x)

int  ext_array(Type elem, int elem_x, int count);   /* elem[count]: its extension */
Type ext_elem(int x);           /* an array's element type */
int  ext_elem_x(int x);         /* and its extension */
int  ext_count(int x);
int  ext_bytes(int x);

int     ext_record(int is_union, NameRef tag);   /* a new, incomplete record */
int     ext_func(Type ret, int ret_x, int first, int count, int declared);
int     ext_func_first(int x);          /* its parameters' run */
int     ext_func_count(int x);
int     ext_func_declared(int x);       /* whether it gives them */
int     ext_func_variadic(int x);       /* and more, with `...` */
void    ext_record_done(int x, int first, int bytes);
int     ext_is_union(int x);
int     ext_complete(int x);
void    ext_set_bits(int x);            /* it has bit-fields */
int     ext_has_bits(int x);
void    ext_set_flex(int x);            /* and an array with no size last */
int     ext_has_flex(int x);
NameRef ext_tag(int x);                 /* NAME_NONE when it has none */

/* A record's members, in the order they were declared: member_first, then
 * member_next until -1. */
int     member_add(NameRef name, Type type, int ext, int offset, int quals);
int     member_quals(int member);
void    member_set_bits(int member, int bits);
int     member_bits(int member);

/* A bit-field: `width` bits from bit `pos` of the `bytes` bytes at its
 * member's offset. */
typedef struct {
    unsigned char pos, width, bytes, is_signed;
} BitField;

int             bitfield_intern(int pos, int width, int is_signed);
const BitField *bitfield_at(int i);
void    member_link(int member, int next);
int     member_first(int x);
int     member_next(int member);
int     member_find(int x, NameRef name);      /* -1 if it has none so named */
NameRef member_name(int member);
Type    member_type(int member);
int     member_ext(int member);
int     member_offset(int member);

/* C promotes anything narrower than int to int before doing arithmetic on it,
 * so a value in a register is always int-wide. Only loads, stores and casts
 * deal in the narrow widths. */
#define type_promote(ty)  ((Type) (type_size(ty) < ACC_INT_SIZE ? TY_INT : (ty)))

/* The type a byte or two of arithmetic may be done in, when the result is
 * on its way into an object of that type: see narrow_dest. Not _Bool, which
 * does not keep the low bits of what it is given: 2 in a _Bool is 1. */
#define type_narrow(ty)   ((Type) (type_size(ty) < ACC_INT_SIZE \
                                   && (ty) != TY_BOOL ? (ty) : 0))

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

/* NameRef is declared with Type, at the top: the struct members below the
 * types are named by one. */

NameRef     name_intern(const char *text, int len);
const char *name_text(NameRef ref);
void        name_init(void);

extern char *name_arena;    /* for name_global, further down */

/* The floating literal at s, as the bits of the nearest float; returns where
 * it ends, which is s if it has no digits. In float.c. */
const char *float_literal(const char *s, uint32_t *bits);
uint32_t    float_from_int(uint64_t magnitude, int negative);

/* Folding a constant expression, in integers rather than in the float of
 * whichever compiler built this one: see the head of the arithmetic in
 * float.c for why the host's own is not good enough. */
uint32_t    float_add(uint32_t a, uint32_t b);
uint32_t    float_mul(uint32_t a, uint32_t b);
uint32_t    float_div(uint32_t a, uint32_t b);
uint32_t    float_neg(uint32_t a);
int64_t     float_to_int(uint32_t a);
int         float_is_nan(uint32_t a);
int         float_is_inf(uint32_t a);
int         float_is_zero(uint32_t a);
int         float_compare(uint32_t a, uint32_t b);  /* -1, 0, 1; 2 unordered */

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

    /* And two that are not macros but are recognised the same way, since
     * the lexer sees them only after they are interned and a code here
     * costs the names that are not them nothing: L, which is a name like
     * any other unless a quote follows it, when it begins a wide literal;
     * and _Pragma, the operator form of #pragma. */
    TK_WIDE, TK_PRAGMA_OP,

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
extern uint32_t tok_val_hi; /* and its high half, when a long long */
extern const char *tok_str; /* its bytes, when TK_STRING, escapes undone */
extern int      tok_str_len; /* and how many, without a terminator */
extern int      tok_str_wide; /* whether they are wchar_t, two bytes each */
extern Type     tok_type;   /* and its type, which C99 fixes by its size */
extern float    tok_fval;   /* its value, when TK_FLOAT */
extern NameRef  tok_name;   /* its name, when TK_IDENT */
extern int      tok_line;   /* the line it started on */
extern int      tok_prev_line; /* where the token before it ended */

void lex_init(void);
void lex_open(const char *path);
void lex_add_include(const char *dir);
void lex_define(const char *arg);       /* -D, as a #define would read it */
void lex_undefine(const char *arg);     /* -U, as a #undef would */

/* Every file the compile read, and enough about each to tell whether it has
 * changed: see the note in src/lex.c. */
void        lex_want_deps(void);
int         lex_ndeps(void);
const char *lex_dep_path(int i);
void        lex_dep_marks(int i, unsigned *size, unsigned *sum,
                          unsigned *weighted);
int         lex_file_marks(const char *path, unsigned *size, unsigned *sum,
                           unsigned *weighted);  /* a -I directory, in order */
void lex_close(void);
void lex_end(void);   /* what has to be finished before the file is */
void next(void);                 /* advance to the following token */
int  lex_colon_follows(void);    /* whether `:` comes after the current token */
int  lex_rparen_follows(void);   /* and whether `)` does */
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
    SYM_LOCAL_STRUCT,   /* a local struct or union: val is its number in the
                         * array area, where it lives as an array does */
    SYM_LOCAL_CONST,    /* a const local or parameter: as SYM_LOCAL, but it
                         * reads as a value, which cannot be assigned to */
    SYM_LOCAL_FAR,      /* a local the frame pointer cannot reach: val is its
                         * number in the array area, where it lives as a
                         * struct does, and every use of it works out its
                         * address. See NEAR_LOCALS */
    SYM_LOCAL_VLA,      /* an array whose length the program works out: val
                         * is the frame offset of the pointer to its room,
                         * taken from the stack where it was declared, and
                         * sym_count is the offset of its size in bytes */
    SYM_FUNC,           /* a function: val is its address in the image */
    SYM_GLOBAL,         /* a file-scope variable: val is its address */
    SYM_GLOBAL_ARRAY,   /* a file-scope array: val is its address, and -1
                         * until something gives it one: a declaration that
                         * only says extern, or one with no size, reserves
                         * nothing */
    SYM_GLOBAL_CONST,   /* a const file-scope variable: as SYM_GLOBAL, read
                         * as a value */

    /* These three are at file scope or in a block, as they are declared:
     * see sym_push_local. */
    SYM_CONST,          /* an enum constant: val is its value */
    SYM_TYPEDEF,        /* a typedef name: type and ext are the type */
    SYM_TAG             /* a struct, union or enum tag: val says which, and
                         * type and ext are the type. Its name is the tag's
                         * with a prefix, which keeps tags apart from other
                         * names: see tag_name */
};

/* Whether a symbol of this kind belongs to the function being compiled
 * rather than to file scope. */
#define sym_kind_local(kind) ((unsigned) (kind) <= SYM_LOCAL_VLA)

/* Ten bytes on the target: a name, a kind, one number whose meaning the
 * kind decides, a type with its extension, and its qualifiers. tinycc's
 * equivalent is 31, and at four hundred lines of input that difference was
 * 40 KB of a 206 KB budget. A width that is not a power of two costs no
 * multiply: a symbol is found by its byte offset into the table, not its
 * position. */
typedef struct {
    NameRef       name;
    unsigned char kind;
    int           val;
    Type          type;         /* fits in what was the pad byte */
    unsigned char ext;          /* the type's extension, when it has one */
    unsigned char quals;        /* SQ_* */
} Sym;

/* What a symbol's declaration said beyond its type. */
enum {
    SQ_CONST    = 1,            /* what is at the bottom of its type is const:
                                 * a pointer's target, an array's elements,
                                 * a struct's members. The same bit as
                                 * VQ_CONST, so a value takes it as it is */
    SQ_REGISTER = 2             /* `register`: its address cannot be taken */
};

/* A function's parameter types, kept beside the symbols. A call converts each
 * argument to the type the parameter was declared with, which matters because
 * a long takes two argument slots where everything else takes one. */
int  sym_params_begin(void);
void sym_param_add(Type type, int ext);
Type sym_param_type(int first, int index);
int  sym_param_ext(int first, int index);
void sym_set_params(int sym, int first, int count);
int  sym_params_first(int sym);
int  sym_nparams(int sym);

/* What is known about a file-scope symbol beyond its type, kept beside its
 * signature. */
enum {
    SYMF_DECLARED = 1,          /* a function: its type is declared, by a
                                 * prototype or its definition, and not
                                 * assumed from a call */
    SYMF_DEFINED  = 2,          /* a function: its body has been read; a
                                 * variable: its initial value has */
    SYMF_PARAMS   = 4,          /* a function: its parameters are declared,
                                 * where `()` in a declaration says nothing
                                 * about them */
    SYMF_VARIADIC = 8,          /* a function: and more of them, `...` */
    SYMF_STATIC   = 32,         /* said static at file scope, so its name is
                                 * this file's alone: it is not put in the
                                 * object for another file to find, and two
                                 * files may each have one */
    SYMF_USED     = 64,         /* a function: something outside the file's
                                 * own functions holds its address, so it
                                 * stays whatever else goes */
    SYMF_EXTERN   = 16          /* a variable: a declaration said extern, so
                                 * some other file defines it. What tells it
                                 * apart from one this file declared and
                                 * never gave a value to, which C says starts
                                 * at zero and acc puts in the bss */
};
void sym_set_flags(int sym, int flags);
void sym_clear_flags(int sym, int flags);
unsigned char sym_flags(int sym);

/* How many elements an array symbol has, kept where a function's signature
 * would be: `&a` needs it to say what it points at. */
void sym_set_count(int sym, int count);
int  sym_count(int sym);

void sym_init(void);
/* Where a variable is, as its symbol's val says it.
 *
 * An address, when something has given it one. -1 when nothing has yet: a
 * declaration that only said extern, or one whose size is not known yet.
 * And otherwise where in the bss it starts, written so that the start of the
 * bss is told apart from having no address at all. */
#define sym_in_bss(v)    ((v) <= -2)
#define sym_bss_at(v)    (-(v) - 2)
#define sym_bss_val(at)  (-(at) - 2)

/* Symbols are referred to by index. A Sym * is only good until the next push,
 * because the table is grown with realloc; see sym.c. */
#define SYM_NONE (-1)

int  sym_nglobals(void);                 /* the file-scope region, in bytes */
int  sym_find(NameRef name);             /* innermost first; SYM_NONE if unknown */
int  sym_push(NameRef name, int kind, int val);
int  sym_push_local(NameRef name, int kind, int val);  /* in the function, whatever the kind */
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
int  sym_declared_in(int sym, int mark);  /* in the scope from mark on; -1 is file scope */

/* ------------------------------------------------------------------ */
/* values                                                              */

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
    VAL_WIDE        /* a constant too wide for val: see wide_const */
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
void vapply(int op, Type narrow);
void vaddr_local(int offset, Type type);  /* &local */
void vderef(void);                    /* *p, replacing the pointer */
void vmember(int offset, Type type, int ext, int quals);  /* p->m, from p */
void vstore_indirect(void);           /* *p = v, with p under v */
void vneg(void);
void vnot(void);
int  vpop_reg(void);                  /* force the top into a register */
void vdrop(void);
void gen_stmt_end(void);              /* the scratch area is free again */
void gen_value_end(void);             /* all of it nothing still holds */

void gen_call(int fn, int nargs, int params_first, int nparams);
void gen_call_indirect(int nargs);      /* through the pointer under them */
void vpush_function(int fn);          /* a function's address */
void vpush_global_addr(int sym);      /* a variable's, when it has none yet */
void gen_data_fixup(int fn, int at);  /* and one in a global's bytes */
extern int gen_data_context;          /* a global's initial value is being read */
extern int gen_pending_sym;            /* whose address it needs, not yet known */

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
    int    rt_any_used;
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
void gen_return(int line);            /* `return`, at the line it is on */
/* What a program has of the Agon's memory: the image, what it leaves at
 * zero, and then its heap and its stack. */
#define ACC_RAM_BYTES 458752

/* Kept between the heap's top and the top of memory, for the stack to come
 * down into. The same figure src/agon.ld reserves for acc's own stack, for
 * the same reason and with as little to go on. */
#define ACC_STACK_RESERVE 16384

/* Variables that start at zero and so take no room in the file. */
int  gen_bss_reserve(int bytes);      /* room in it; returns where */
void gen_bss_symbol(int sym, int at); /* and which symbol is there */
int  gen_bss_offset(int sym);         /* where one is, or -1 */
int  gen_bss_used(int at, int bytes); /* whether anything reaches into it */
void gen_bss_forget(int sym);         /* it is not in the bss after all */
void gen_bss_fixup(int at);           /* a slot that wants the bss's start */
int  gen_nbss_fixups(void);
int  gen_bss_fixup_at(int i);
int  gen_bss_len(void);

void gen_finish(void);          /* resolve calls to functions defined later */

/* What is still waiting on something, which is what a link takes to a
 * library to ask whether it has it. */
int gen_nfixups(void);
int gen_fixup_sym(int i);
int gen_no_address(int sym);

/* -c: what this file could not resolve, for the object to hand on. */
extern int gen_objects;
int gen_nexterns(void);
int gen_extern_at(int i);
int gen_extern_sym(int i);
void gen_startup(int by_exit, const char *program); /* the entry stub */

/* ------------------------------------------------------------------ */
/* output                                                              */

void out_open(const char *path, int header);
void out_close(void);
void out_free(void);                /* let it go without writing it */
int  out_len(void);                 /* bytes written so far */

/* Where the image is loaded. Set before out_open and not after: every address
 * the compiler writes is absolute and is worked out from this. */
extern int out_base;

/* The three bytes at `at` are an address inside the image, and would have to
 * change if it were loaded somewhere else. Called where the slot is emitted
 * rather than where it is filled in, so the offsets come out in order and a
 * rewind can drop the ones it undoes.
 *
 * Inlined, and the table's three pointers visible for it, for the reason the
 * byte emitters are: see src/out.c. */
extern int *out_relocs, *out_reloc_put, *out_reloc_limit;
void out_reloc_grow(void);
void out_reloc_back(int at);
void out_reloc_merge(const int *add, int n, int first);

static inline __attribute__((always_inline))
void out_reloc(int at)
{
    if (out_reloc_put == out_reloc_limit)
        out_reloc_grow();
    at -= out_base;
    /* Unsigned, and reading a slot that is always there: an offset is never
     * negative, so the two compare the same either way, and a signed one is
     * a helper call on this target. The table starts with a sentinel so that
     * the first relocation has something behind it to compare with -- see
     * src/out.c. */
    if ((unsigned) out_reloc_put[-1] > (unsigned) at) {
        out_reloc_back(at);

        return;
    }
    *out_reloc_put++ = at;
}

int  out_nrelocs(void);
int  out_reloc_at(int i);

/* A run of bytes taken out of the image: see out_cut. Two passes take runs
 * out -- the one that leaves out the functions a file does not use, and the
 * one that shortens a jump whose target is near -- and both hand the runs to
 * out_cut_sum first, which works out what each one moves everything after it
 * by and keeps that beside them. */
typedef struct {
    int at, len;
} Cut;

void out_cut_sum(const Cut *cuts, int n);   /* before out_cut_moved or out_cut */
void out_cut(const Cut *cuts, int n, int first);
int  out_cut_moved(int a);          /* an address, wherever it is */
void out_cut_rewind(void);          /* and a run of them in rising order */
int  out_cut_next(int a);
void out_relocs_write(const char *path);

/* The output's write position and its end, and the growth that happens a
 * dozen times a compile. Visible so that the three emitters below can be
 * inlined: every byte the compiler emits goes through one of them, and as
 * calls each opened a frame on this target to do a compare and a store. */
extern unsigned char *out_put, *out_limit;
void out_grow(void);

/* The first byte of the image, and the address the next one will have.
 *
 * out_here is asked wherever the compiler needs to know where it is -- every
 * jump, every function, every global, and every relocation recorded beside
 * one of those -- and as a call it opened a frame to do a subtraction and an
 * add. */
extern unsigned char *out_img;

static inline __attribute__((always_inline))
int out_here(void)
{
    return out_base + (int) (out_put - out_img);
}

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
 * call, a jump and every load of a constant. Four bytes, one bounds check.
 *
 * The room left is compared unsigned, as in the two below: it is never
 * negative, and a signed compare is a helper call on this target, in front
 * of every byte the compiler emits. */
static inline __attribute__((always_inline))
void out_opcode24(int opcode, int value)
{
    if ((unsigned) (out_limit - out_put) < 4)
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
    if ((unsigned) (out_limit - out_put) < 2)
        out_grow();
    out_put[0] = (unsigned char) first;
    out_put[1] = (unsigned char) second;
    out_put += 2;
}

static inline __attribute__((always_inline)) void out_byte3(int first, int second, int third)
{
    if ((unsigned) (out_limit - out_put) < 3)
        out_grow();
    out_put[0] = (unsigned char) first;
    out_put[1] = (unsigned char) second;
    out_put[2] = (unsigned char) third;
    out_put += 3;
}
void out_word24(int v);
void out_rewind(int here);          /* forget what came after out_here() was here */
void out_seek(int here);            /* back to it, keeping what came after */
void out_copy(int at, unsigned char *to, int len);  /* bytes already written */
int  out_read24(int at);            /* and read one back */
void out_patch24(int at, int v);

/* ------------------------------------------------------------------ */
/* objects                                                             */

/* What a symbol in an object is: OBJ_DEFINED when this object has the thing
 * and says where, and OBJ_FUNC when it is a function rather than data. */
enum {
    OBJ_DEFINED = 1,
    OBJ_FUNC    = 2,
    OBJ_BSS     = 4             /* a variable with no room in the text: its
                                 * value is where in this object's bss it
                                 * starts, and what address that comes to is
                                 * the linker's to work out */
};

/* An object read in: the file's bytes, and where each part of it starts.
 * Nothing is copied out -- the accessors below read the bytes in place. */
typedef struct {
    unsigned char *all;
    const char    *path;
    unsigned char *syms, *relocs, *deps, *text;
    char          *strings;
    int            build;    /* the acc that made it: see src/build_id.sh */
    int            text_len, bss_len, nsyms, nrelocs, ndeps, strings_len;
} Object;

/* A library: objects end to end, with a list of what each defines in front
 * of them. Only that list is read when one is opened; a member is read when
 * the link turns out to want it. */
typedef struct {
    const char    *path;
    unsigned char *front;                /* the list, as it is in the file */
    unsigned char *members, *defs;
    char          *strings;
    int            build, nmembers, ndefs, strings_len, size;
} Archive;

void        ar_write(const char *path, const char **members, int nmembers);
void        ar_open(const char *path, Archive *a);
void        ar_close(Archive *a);
int         ar_find(const Archive *a, const char *name);  /* which member, or -1 */
void        ar_member(const Archive *a, int i, Object *o);
const char *ar_member_name(const Archive *a, int i);

void obj_write(const char *path);
void obj_take(unsigned char *all, int len, const char *path, Object *o);
int  obj_current(const char *path, const char *source);
void obj_read(const char *path, Object *o);
void obj_free(Object *o);

const char *obj_sym_name(const Object *o, int i);
int         obj_sym_value(const Object *o, int i);
int         obj_sym_flags(const Object *o, int i);
int         obj_reloc_at(const Object *o, int i);
int         obj_reloc_sym(const Object *o, int i);   /* 0, or one more than a symbol */
const char *obj_dep_path(const Object *o, int i);
void        obj_dep_marks(const Object *o, int i, unsigned *size,
                          unsigned *sum, unsigned *weighted);

#endif /* ACC_H */
