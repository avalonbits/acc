/*
 * Symbols: what a name stands for in each scope, and the extension
 * words a type carries. See sym.c.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_SYM_H
#define ACC_SYM_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lex.h"

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

/* Fifteen bytes on the target: a name, a kind, one number whose meaning
 * the kind decides, a type with its extension, and its qualifiers; then
 * what a function takes -- where its parameters' types start, how many --
 * and the SYMF_* flags. tinycc's equivalent is 31, and at four hundred
 * lines of input that difference was 40 KB of a 206 KB budget. A width that
 * is not a power of two costs no multiply: a symbol is found by its byte
 * offset into the table, not its position.
 *
 * The last three were a table of their own, as wide as this one so that
 * the same offset found both: twenty bytes a symbol, in two tables that
 * each doubled on their own. In one record it is fifteen, and one table. */
typedef struct {
    NameRef       name;
    unsigned char kind;
    int           val;
    Type          type;         /* fits in what was the pad byte */
    unsigned char ext;          /* the type's extension, when it has one */
    unsigned char quals;        /* SQ_* */
    int           first;        /* the first parameter's type, or a count */
    unsigned char count;        /* how many parameters */
    unsigned char flags;        /* SYMF_* */
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
    SYMF_INLINE   = 128,        /* a function: static inline, and its body
                                 * one return that a call can be compiled
                                 * as, in place: see inline_keep */
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
extern int sym_nbytes, sym_nglobal_bytes;       /* see src/sym.c */

static inline int sym_scope_begin(void)
{
    return sym_nbytes - sym_nglobal_bytes;
}

void sym_scope_end(int mark);
int  sym_find_in(NameRef name, int mark);  /* a local in the scope from mark on */
int  sym_push_param(NameRef name, int val, int mark);  /* SYM_NONE: named twice */
int  sym_declared_in(int sym, int mark);  /* in the scope from mark on; -1 is file scope */

#endif
