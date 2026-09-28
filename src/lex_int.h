/*
 * What the front end's files share among themselves: names.c, source.c,
 * macro.c, directive.c and lex.c. What the rest of acc sees of them is in
 * acc.h; this is their state and their calls to each other.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_LEX_INT_H
#define ACC_LEX_INT_H

#include <stdio.h>

#include "acc.h"

/* The files an `#include` is inside, innermost last.
 *
 * The window's state stays in plain variables of source.c, declared below,
 * rather than in a field of whatever is on top of this stack: the lexer's inner loops read
 * `cursor` on every character, and reaching it through a pointer costs that
 * loop a load it does not have. Pushing a file puts the statics here and
 * starts new ones; popping puts them back.
 *
 * Eight deep, because each level holds a window of its own and the memory
 * is what there is least of: a program 128 KB of buffer deep in headers has
 * other problems. */
#define INCLUDE_MAX 8

typedef struct {
    char       *src, *cursor, *src_end, *src_raw;
    int         cap;
    long        at;             /* where the handle was when it was closed */
    char        src_held;
    FILE       *src_file;
    const char *src_path;
    const char *src_real;
    int         line;
    int         owned;          /* OWN_BUF and OWN_PATH, below */
    NameRef     macro;          /* the macro whose text this level is */
    int         conds;          /* conditionals open when this was pushed */
    int         dep;            /* which file of dep_files this is, or -1 */
    const char *use;            /* src_use, below */
} Source;

/* What this level allocated, and which macro it is the expansion of.
 *
 * A file owns both its window and the copy of its name; a macro with
 * parameters owns the text its expansion was built into, and has no name of
 * its own; a macro without them owns neither, since its text is the
 * definition in the table. The two are separate because a file's handle is
 * closed as soon as it has been read to its end, which is before the level
 * is popped -- so whether there is still a handle says nothing about
 * whether there is a name to free. */
#define OWN_BUF   1

#define OWN_PATH  2

/* Where a `<...>` include is looked for, and a `"..."` one after the
 * directory of the file that asked for it. From -I, in the order given. */
#define INCLUDE_DIRS 8

/* The most parameters a macro may take, and so the most arguments a call
 * may pass. */
#define PARAMS_MAX 127          /* what C99 5.2.4.1 asks a compiler to take */

typedef struct {
    NameRef  name;              /* NAME_NONE in an empty slot */
    char    *text;
    NameRef *params;            /* null when the macro takes none */
    short    nparams;           /* -1 when it is a name and not a call */
    short    variadic;          /* whether the last parameter is `...` */
    int      serial;            /* which definition: see lex_macro_def */
} Macro;

/* A buffer that grows, for the text an expansion is built into. On the
 * heap rather than in a frame: the Agon's stack and heap grow towards each
 * other out of one region, and a few hundred bytes of local buffer in
 * something that can call itself is how that ends badly. */
typedef struct {
    char *text;
    int   len, cap;
} Buf;

/* Whether a name is a macro, kept in the byte four in front of its text.
 *
 * Every identifier in the program asks this, and for nearly all of them the
 * answer is no. It used to be asked of the macro table, whenever the program
 * had defined anything at all: a hash, a probe and a frame for every
 * identifier -- and the hash was a 24-bit AND and the probe an index into
 * entries that are not a power of two wide, a runtime call each. That was 4%
 * of compiling big.c, for names that were almost never macros. A byte in
 * front of the name is one load.
 *
 * The byte is two bits. NAME_MACRO is whether the name is a macro;
 * NAME_WIDE is on L alone, and says to look at what follows it, since L
 * and a quote are a wide literal and not a name at all. Either sends the
 * name to expand(), out of line, which is where the question is answered;
 * the lexer's own path is the one load either way. */
#define name_is_macro(ref)  (name_arena[(ref) - 4])

#define NAME_MACRO  1

#define NAME_WIDE   2

/* The conditionals open at this moment, and where each of them began. A
 * file has to close its own, so pushing and popping a source looks at
 * these; they are defined with the rest of the conditional machinery. */
typedef struct {
    unsigned char taken;
    unsigned char seen_else;
    int           line;
} Cond;

#define COND_MAX 16

/* A call's arguments start with room for this many and the null after them,
 * and grow when a macro takes more: most take a few, and C99 asks for 127,
 * which as the first room would be zeroed on every call. */
#define ARGS_FIRST 17

/* source.c */
extern char *src;
extern char *cursor;
extern char *src_end;
extern char *src_raw;
extern char src_held;
extern FILE *src_file;
extern const char *src_path;
extern const char *src_real;
extern int line;
extern int src_owned;
extern NameRef src_macro;
void opt_fold(char kind, const char *arg);
extern Source open_files[INCLUDE_MAX];
extern int depth;
extern const char *include_dirs[INCLUDE_DIRS];
extern int ninclude_dirs;
int ucn_at(const char *r);
int refill(void);
void push_source(const char *path);
void push_text(NameRef macro, char *text, int len);
void push_owned_text(NameRef macro, char *text);
int pop_source(void);
int column_of(const char *p);
__attribute__((noinline)) void window_more(void);

/* macro.c */
void buf_put(Buf *b, const char *s, int n);
void buf_putc(Buf *b, int c);
extern Macro **macros;
extern unsigned nmacro_slots, nmacros;
extern Macro *macro_spare;
extern void *macro_chunks;
extern char *macro_at, *macro_end;
Macro *macro_find(NameRef name);
void macro_define(NameRef name, const char *text, int len, NameRef *params, int nparams, int variadic);
void macro_undef(NameRef name);
char **args_room(char **argv, int argc, int *cap);
void free_args(char **argv, int argc);
int hash_at(const char *p);
char *build_expansion(Macro *m, char **argv, int argc);
extern const char date_text[];
extern const char time_text[];
__attribute__((noinline)) void predefined(void);
__attribute__((noinline)) int expand(NameRef name);
int predefined_name(NameRef name);
void predefined_macros_init(void);

/* directive.c */
extern Cond conds[COND_MAX];
extern int nconds;
void once_add(const char *path);
void expand_text_into(Buf *out, const char *text);
void do_define(void);
void do_undef(void);
__attribute__((noinline)) void directives(void);

/* lex.c */
__attribute__((noinline)) void skip_comment(void);
extern NameRef kw_limit;
__attribute__((noinline)) void skip_space(void);
void pragma_operator(void);
void wide_literal(void);

/* Whether `at` is the first thing on its line, blanks aside. Only asked of
 * a '#', so the walk is off every path but that one.
 *
 * Always inlined. next() asks it, and when lex_digraph came to ask it too
 * clang made it a function of its own, and next() -- where it had been
 * inlined -- was given different registers for the loop that reads a name:
 * 1% of every compile, spent in a loop that does not call this at all. */
static inline __attribute__((always_inline))
int at_line_start(const char *at)
{
    while (at > src && (at[-1] == ' ' || at[-1] == '\t' || at[-1] == '\r'))
        at--;

    return at == src || at[-1] == '\n';
}

#endif
