/*
 * Macros: the table of them, collecting a call's arguments, building and
 * rescanning an expansion, the predefined ones, and those given on the
 * command line.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#include "ctype.h"
#include "lex_int.h"

/* ------------------------------------------------------------------ */
/* macros                                                              */

/* What a name stands for, and the text it stands for. The text is kept as
 * it was written rather than as tokens: expanding a macro then costs no
 * more than pushing that text as another window and reading it, which is
 * the machinery `#include` already needed. Rescanning falls out of it --
 * the lexer simply carries on -- and so does a macro that uses a macro.
 *
 * Open addressing on the name's offset, which is already a good spread: the
 * names came out of a hash table. Every identifier in the program asks this
 * question, so the first thing asked is whether there are any macros at
 * all. */

static int macro_serials;

void buf_put(Buf *b, const char *s, int n)
{
    if (b->len + n + 1 > b->cap) {
        int want = b->cap ? b->cap * 2 : 128;

        while (want < b->len + n + 1)
            want *= 2;
        b->text = realloc(b->text, (size_t) want);
        if (!b->text)
            acc_error("out of memory expanding a macro");
        b->cap = want;
    }
    memcpy(b->text + b->len, s, (size_t) n);
    b->len += n;
    b->text[b->len] = '\0';
}

void buf_putc(Buf *b, int c)
{
    char ch = (char) c;

    buf_put(b, &ch, 1);
}

/* The table is of pointers, and the records are in chunks of their own.
 * It was of the 16-byte records themselves, kept under three quarters
 * full: an Agon program's headers, 500-odd macros, made it 16 KB, and
 * growing it held that and 32 KB more at once. As pointers the table is 3
 * KB, the records are 16 bytes for each macro there is, and growing the
 * table copies the pointers, not the records -- which also means a Macro *
 * stays good across a #define that grows it. A record an #undef frees goes
 * on a list for the next #define. */
Macro  **macros;
unsigned nmacro_slots, nmacros;
Macro   *macro_spare;            /* freed records, through `text` */

#define MACRO_CHUNK 1024

/* A chunk starts with the one before it, so lex_close can free them all. */
void  *macro_chunks;
char  *macro_at, *macro_end;

static Macro *macro_new(void)
{
    Macro *m;

    if (macro_spare) {
        m = macro_spare;
        macro_spare = (Macro *) (void *) m->text;

        return m;
    }
    if ((size_t) (macro_end - macro_at) < sizeof *m) {
        void **chunk = malloc(MACRO_CHUNK);

        if (!chunk)
            acc_error("out of memory for macros");
        *chunk = macro_chunks;
        macro_chunks = chunk;
        macro_at = (char *) chunk + sizeof(Macro *) * 2;
        macro_end = (char *) chunk + MACRO_CHUNK;
    }
    m = (Macro *) (void *) macro_at;
    macro_at += sizeof *m;

    return m;
}

static void macros_grow(void);

/* The slot a name belongs in, empty or not. */
static Macro **macro_slot(NameRef name)
{
    unsigned i = (name >> 2) & (nmacro_slots - 1);

    while (macros[i] && macros[i]->name != name)
        i = (i + 1) & (nmacro_slots - 1);

    return &macros[i];
}

/* The definition of a name, or null. */
Macro *macro_find(NameRef name)
{
    if (!nmacros)
        return NULL;

    return *macro_slot(name);
}

/* Which definition a name has as a macro now, or 0: the same name defined
 * again is a different definition. Numbered, rather than told apart by where
 * the text is, because a definition freed by #undef and one made after it
 * can be given the same memory. */
int lex_macro_def(NameRef name)
{
    Macro *m = macro_find(name);

    return m ? m->serial : 0;
}

static void macros_grow(void)
{
    Macro  **old = macros;
    unsigned n = nmacro_slots, i;

    nmacro_slots = n ? n * 2 : 64;
    macros = calloc(nmacro_slots, sizeof *macros);
    if (!macros)
        acc_error("out of memory for macros");
    for (i = 0; i != n; i++)
        if (old[i])
            *macro_slot(old[i]->name) = old[i];
    free(old);
}

void macro_define(NameRef name, const char *text, int len,
                         NameRef *params, int nparams, int variadic)
{
    Macro **slot, *m;
    char   *keep;

    /* Kept under half full: past that a linear probe starts walking. */
    if (!nmacro_slots || nmacros * 2 >= nmacro_slots)
        macros_grow();

    keep = malloc((size_t) len + 1);
    if (!keep)
        acc_error("out of memory for a macro");
    /* Asked, because `#define X` with nothing after it is a macro of no
     * bytes and there is nothing to point at for it -- and memcpy from a
     * null pointer is undefined even when it is told to copy nothing. */
    if (len)
        memcpy(keep, text, (size_t) len);
    keep[len] = '\0';

    slot = macro_slot(name);
    m = *slot;
    if (m) {
        free(m->text);          /* defined again; C allows it if it matches */
        free(m->params);
    } else {
        m = macro_new();
        *slot = m;
        nmacros++;
    }
    m->name = name;
    m->text = keep;
    m->serial = ++macro_serials;
    m->params = params;
    m->nparams = (short) nparams;
    m->variadic = (short) variadic;
    name_is_macro(name) |= NAME_MACRO;
#ifdef OPT_ACC
    prescan_macro(m);
#endif
}

/* Forgotten, and the slot left usable. A tombstone is not needed: the run of
 * names that probed past this one is put back through the table. */
void macro_undef(NameRef name)
{
    Macro  **slot, *m;
    unsigned i;

    if (!nmacros)
        return;
    slot = macro_slot(name);
    m = *slot;
    if (!m)
        return;
    free(m->text);
    free(m->params);
    m->params = NULL;
    m->text = (char *) (void *) macro_spare;
    macro_spare = m;
    *slot = NULL;
    nmacros--;
    name_is_macro(name) &= ~NAME_MACRO;

    /* Whatever follows in this run may have probed past the hole. */
    i = (unsigned) (slot - macros);
    for (;;) {
        Macro *moved;

        i = (i + 1) & (nmacro_slots - 1);
        if (!macros[i])
            return;
        moved = macros[i];
        macros[i] = NULL;
        *macro_slot(moved->name) = moved;
    }
}

/* Whether a macro is already being expanded, which is what stops `#define A
 * A` from going on for ever. The standard paints the tokens rather than the
 * region, so a name that comes back out of its own expansion and is then
 * used again further along is not expanded here where it would be; the
 * difference needs a macro that expands to its own name, which no program
 * that means anything contains. */
static int expanding(NameRef name)
{
    int i;

    if (src_macro == name)
        return 1;
    for (i = 0; i < depth; i++)
        if (open_files[i].macro == name)
            return 1;

    return 0;
}

/* More of the file, and whatever white space and comments begin it. At the
 * end of the file the cursor is left on the sentinel, which is what next()
 * reads as the end.
 *
 * Out of line and off next()'s path: it runs once per 16 KB, and next() is
 * where a compile spends its time. */
/* ------------------------------------------------------------------ */
/* macros with parameters                                              */

/* Whether a '(' comes next, over space and over the end of whatever window
 * we are in: `f` and its `(` may come from different places, and a name
 * that is a function-like macro with no '(' after it is not a use of it at
 * all but an ordinary identifier.
 *
 * Only space is stepped over, which is never wrong to step over. */
static int paren_follows(void)
{
    for (;;) {
        const char *p = cursor;

        while (is_space(*p))
            p++;
        if (*p)
            return *p == '(';

        /* The window ended. Reading more is safe here: whatever comes back
         * is still ahead of the cursor, and nothing has been consumed. */
        if (!refill() && !pop_source())
            return 0;
    }
}

/* One character of the argument list, with the window moved on when it
 * runs out. Arguments can run over lines and, when one macro's expansion
 * ends inside another's argument list, over the end of a window. */
static int args_char(void)
{
    while (!*cursor) {
        if (!refill() && !pop_source())
            acc_error_at(line, "a macro's arguments are not closed");
    }
    if (*cursor == '\n')
        line++;

    return (unsigned char) *cursor++;
}

/* Room for argument `argc` and the null after it. */
char **args_room(char **argv, int argc, int *cap)
{
    int more;

    if (argc + 1 < *cap)
        return argv;
    more = *cap * 2;
    argv = realloc(argv, (size_t) more * sizeof *argv);
    if (!argv)
        acc_error("out of memory for a macro's arguments");
    memset(argv + *cap, 0, (size_t) (more - *cap) * sizeof *argv);
    *cap = more;

    return argv;
}

/* The arguments of a call, each as the text between the commas. Nesting is
 * counted so that a comma inside parentheses or brackets belongs to what it
 * is inside, and strings and character constants go over whole. */
static char **collect_args(Macro *m, int *out_argc)
{
    char **argv;
    Buf    arg;
    int    argc = 0, depth = 0, i, cap = ARGS_FIRST;

    argv = calloc(ARGS_FIRST, sizeof *argv);
    if (!argv)
        acc_error("out of memory for a macro's arguments");
    arg.text = NULL; arg.len = 0; arg.cap = 0;

    while (is_space(*cursor) || !*cursor) {
        if (!*cursor) {
            if (!refill() && !pop_source())
                acc_error_at(line, "a macro's arguments are not closed");

            continue;
        }
        if (*cursor == '\n')
            line++;
        cursor++;
    }
    cursor++;                           /* the '(' */

    for (;;) {
        int c = args_char();

        if (c == '(' || c == '[') {
            depth++;
        } else if (c == ')' && depth == 0) {
            break;
        } else if (c == ')' || c == ']') {
            depth--;
        } else if (c == ',' && depth == 0
                   && !(m->variadic && argc == m->nparams)) {
            /* The comma that ends one argument -- unless the variadic one
             * has started, where the commas are part of it. */
            if (argc == PARAMS_MAX)
                acc_error_at(line, "a macro takes at most %d arguments",
                             PARAMS_MAX);
            if (argc + 1 >= cap)
                argv = args_room(argv, argc, &cap);
            argv[argc++] = arg.text ? arg.text : strdup("");
            arg.text = NULL; arg.len = 0; arg.cap = 0;

            continue;
        } else if (c == '"' || c == '\'') {
            int quote = c;

            buf_putc(&arg, c);
            for (;;) {
                c = args_char();
                buf_putc(&arg, c);
                if (c == '\\') {
                    buf_putc(&arg, args_char());

                    continue;
                }
                if (c == quote)
                    break;
            }

            continue;
        }

        /* Runs of space become one, so that what a `#` makes of an argument
         * is the same however it was written. */
        if (is_space(c)) {
            if (arg.len && !is_space((unsigned char) arg.text[arg.len - 1]))
                buf_putc(&arg, ' ');

            continue;
        }
        buf_putc(&arg, c);
    }

    if (argc + 1 >= cap)
        argv = args_room(argv, argc, &cap);
    argv[argc++] = arg.text ? arg.text : strdup("");

    /* `f()` on a macro that takes nothing passes nothing, not one argument
     * that happens to be empty. On one that takes a parameter the same text
     * does pass an empty argument, which is why the macro decides. */
    if (m->nparams == 0 && argc == 1 && !*argv[0]) {
        free(argv[0]);
        argv[0] = NULL;
        argc = 0;
    }

    /* Blanks at either end are not part of an argument. */
    for (i = 0; i != argc; i++) {
        char *a = argv[i];
        int   n = (int) strlen(a);

        while (n && a[n - 1] == ' ')
            a[--n] = '\0';
        if (*a == ' ')
            memmove(a, a + 1, strlen(a));
    }

    *out_argc = argc;

    return argv;
}

void free_args(char **argv, int argc)
{
    int i;

    for (i = 0; i != argc; i++)
        free(argv[i]);
    free(argv);
}

/* Which parameter a name is, or -1. */
static int param_index(Macro *m, const char *at, int len)
{
    int i;

    for (i = 0; i != m->nparams; i++) {
        const char *p = name_text(m->params[i]);

        if ((int) strlen(p) == len && !memcmp(p, at, (size_t) len))
            return i;
    }
    if (m->variadic && len == 11 && !memcmp(at, "__VA_ARGS__", 11))
        return m->nparams;

    return -1;
}

static void put_expanded(Buf *out, const char *text)
{
    expand_text_into(out, text);
}

/* An argument as the string it was written as, for `#`. */
static void put_stringified(Buf *out, const char *text)
{
    buf_putc(out, '"');
    while (*text) {
        if (*text == '"' || *text == '\\')
            buf_putc(out, '\\');
        buf_putc(out, *text++);
    }
    buf_putc(out, '"');
}

/* The body with the arguments put in: what the call becomes.
 *
 * `#p` is the argument as it was written, quoted. `a ## b` joins what is on
 * either side of it with nothing between, and the result is read as tokens
 * like anything else, since the buffer this builds is lexed from. */
/* '#' and '##', or their digraphs '%:' and '%:%:' (C99 6.4.6): how many
 * characters the one at p is spelled with, or 0. `#%:` is not a '##' but
 * two '#', as C tokenizes it. */
int hash_at(const char *p)
{
    if (*p == '#')
        return 1;

    return p[0] == '%' && p[1] == ':' ? 2 : 0;
}

static int hashhash_at(const char *p)
{
    if (p[0] == '#' && p[1] == '#')
        return 2;

    return p[0] == '%' && p[1] == ':' && p[2] == '%' && p[3] == ':' ? 4 : 0;
}

/* Whether two characters side by side could be one token that they were
 * not: both punctuation that joins -- `-` and `-`, `<` and `=`. */
static int punct_pair(int a, int b)
{
    return a && b && strchr("+-*/%<>=!&|^.#:", a) && strchr("+-*/%<>=!&|^.#:", b);
}

static void buf_insert_space(Buf *b, int at)
{
    buf_putc(b, ' ');
    memmove(b->text + at + 1, b->text + at, (size_t) (b->len - at - 1));
    b->text[at] = ' ';
}

char *build_expansion(Macro *m, char **argv, int argc)
{
    const char *p = m->text;
    Buf out;

    out.text = NULL; out.len = 0; out.cap = 0;

    while (*p) {
        const char *start;
        int idx, paste_before = 0, n;

        if (*p == '"' || *p == '\'') {          /* whole, names and all */
            int quote = *p;

            start = p++;
            while (*p && *p != quote) {
                if (*p == '\\' && p[1])
                    p++;
                p++;
            }
            if (*p)
                p++;
            buf_put(&out, start, (int) (p - start));

            continue;
        }

        if ((n = hashhash_at(p)) != 0) {
            /* Joined: whatever was put last stays, and the space before it
             * goes, so that the two sides end up next to each other. */
            while (out.len && out.text[out.len - 1] == ' ')
                out.text[--out.len] = '\0';
            p += n;
            while (*p == ' ' || *p == '\t')
                p++;
            paste_before = 1;
        }

        if (!paste_before && (n = hash_at(p)) != 0) {
            const char *q = p + n;

            while (*q == ' ' || *q == '\t')
                q++;
            start = q;
            while (is_alnum((unsigned char) *q))
                q++;
            idx = start == q ? -1 : param_index(m, start, (int) (q - start));
            if (idx < 0)
                acc_error_at(line, "'#' in a macro needs one of its "
                                   "parameters after it");
            put_stringified(&out, idx < argc ? argv[idx] : "");
            p = q;

            continue;
        }

        if (!is_alpha((unsigned char) *p)) {
            buf_putc(&out, *p++);

            continue;
        }

        start = p;
        while (is_alnum((unsigned char) *p))
            p++;

        /* `L'1'` and `L"x"` are one token, a wide literal, and its L is not
         * a name: a parameter called L is not put in its place. The quote
         * is copied whole on the next turn. */
        if (p - start == 1 && *start == 'L' && (*p == '\'' || *p == '"')) {
            buf_putc(&out, 'L');

            continue;
        }
        idx = param_index(m, start, (int) (p - start));
        if (idx < 0) {
            buf_put(&out, start, (int) (p - start));

            continue;
        }

        /* A parameter. Next to a `##` it goes in as it was written; on its
         * own it goes in expanded. */
        {
            const char *arg = idx < argc ? argv[idx] : "";
            const char *q = p;
            int at = out.len, paste_after;

            while (*q == ' ' || *q == '\t')
                q++;
            paste_after = hashhash_at(q) != 0;
            if (paste_before || paste_after)
                buf_put(&out, arg, (int) strlen(arg));
            else
                put_expanded(&out, arg);

            /* The argument's tokens and the body's are separate tokens,
             * and the text they are kept as must not let them run
             * together where they meet: `#define NEG(x) -x` with -1 is
             * `- -1` and not `--1`, and c-testsuite's 00202 pastes a `+`
             * to nothing just before the body's own `+`. A `##` joins on
             * purpose, so not there. */
            if (!paste_before && at > 0 && out.len > at
                && punct_pair(out.text[at - 1], out.text[at]))
                buf_insert_space(&out, at);
            if (!paste_after && out.len > 0 && punct_pair(out.text[out.len - 1], *p))
                buf_putc(&out, ' ');
        }
    }

    if (!out.text)
        out.text = strdup("");

    return out.text;
}

/* __DATE__ and __TIME__.
 *
 * C says both stand for when the translation unit was translated, and lets
 * an implementation that cannot find that out supply a valid date of its
 * own instead. What acc supplies is when acc itself was built, which is
 * the date the compiler that built it gave it -- and, when acc is built by
 * acc, the date this reports.
 *
 * Not the clock, on purpose. Reading it is not free: on the Agon it is a
 * call into MOS, and doing it up front cost more than compiling a small
 * file and made every compile take a different number of cycles from the
 * last, which is a compiler that cannot be measured. It also makes a
 * compile unrepeatable -- a program that names either would compile to
 * different bytes every second, where acc's own tests rest on the same
 * source giving the same image twice. A date that is fixed for a given acc
 * is worth more here than one that is right to the second.
 */
const char date_text[] = __DATE__;
const char time_text[] = __TIME__;

__attribute__((noinline))
void predefined(void)
{
    if (tok == TK_PRAGMA_OP) {
        pragma_operator();

        return;
    }

    /* C99 6.10.8. __STDC_HOSTED__ is 0: a hosted implementation is one with
     * all of the library, and until acc's has every header it is not one.
     * agondev says 1. */
    if (tok == TK_STDC_VERSION) {
        tok_val = 199901L;
        tok = TK_INT;
        tok_type = TY_LONG;
        tok_val_hi = 0;

        return;
    }
    if (tok == TK_LINE || tok == TK_STDC || tok == TK_STDC_HOSTED) {
        tok_val = tok == TK_LINE ? line : tok == TK_STDC ? 1 : 0;
        tok = TK_INT;
        tok_type = TY_INT;
        tok_val_hi = 0;

        return;
    }

    tok_str = tok == TK_FILE ? (src_path ? src_path : "")
            : tok == TK_DATE ? date_text : time_text;
    tok = TK_STRING;
    tok_str_len = (int) strlen(tok_str);
    tok_str_wide = 0;
}

__attribute__((noinline))
int expand(NameRef name)
{
    Macro *m;

    /* L, and a quote after it: the literal is the token, and the answer is
     * that nothing was expanded -- next() has set tok to a name already,
     * and returns what this leaves there instead. Before the macro is
     * looked for, as L"x" is a string even where L is a macro. */
    if ((name_is_macro(name) & NAME_WIDE)
        && (*cursor == '"' || *cursor == '\'')) {
        wide_literal();

        return 0;
    }
    m = macro_find(name);

    if (!m || expanding(name))
        return 0;

    /* A macro with parameters is only used where a '(' follows it. Written
     * on its own the name is an ordinary identifier, which is what lets a
     * function and a macro over it share a name. */
    if (m->nparams >= 0) {
        char **argv;
        char  *text;
        int    argc;

        if (!paren_follows())
            return 0;

        argv = collect_args(m, &argc);
        if (argc < m->nparams || (argc > m->nparams && !m->variadic))
            acc_error_at(line, "'%s' takes %d argument%s, not %d",
                         name_text(name), m->nparams,
                         m->nparams == 1 ? "" : "s", argc);

        text = build_expansion(m, argv, argc);
        free_args(argv, argc);

        if (!*text) {
            free(text);

            return 1;           /* it came to nothing */
        }
        push_owned_text(name, text);

        return 1;
    }

    /* An empty definition expands to nothing at all, and a window over no
     * text is not worth pushing: saying it was expanded is enough, and the
     * caller reads straight past it. `#define EMPTY` and then `EMPTY;` is
     * a `;` on its own. */
    if (*m->text)
        push_text(name, m->text, (int) strlen(m->text));

    return 1;
}

/* Whether a name is one the compiler defines rather than the program:
 * `defined(__LINE__)` is true, and neither may be defined or undefined. */
int predefined_name(NameRef name)
{
    return name < kw_limit
           && (unsigned char) name_arena[name - 3] >= TK_FILE
           && (unsigned char) name_arena[name - 3] <= TK_STDC_HOSTED;
}

/* The macros a compiler is expected to have defined before it reads a line.
 *
 * None of them is in C. What they are is the way a program written for a
 * machine it has not been told about asks what that machine is: <stdint.h>
 * is written in terms of them, and so is every test that wants an integer
 * of a named width without a header to get it from. A compiler that does
 * not have them is one such a program cannot be compiled by, and of the
 * gcc torture tests acc turns down, more are turned down for the want of
 * these than for anything else.
 *
 * The values are agondev's, read out of its clang with `-dM -E`, because
 * they have to be: a program compiled by one and then the other has to be
 * the same program, and these say how wide everything is. They are macros
 * and not words of the language because what most of them stand for is two
 * or three tokens, and because a program is allowed to take them back --
 * gcc lets a file #undef them, and so does this.
 */
static const struct {
    const char *name;
    const char *text;
} predefined_macros[] = {
    /* What a type of a given width is called here. */
    { "__SIZE_TYPE__",       "unsigned int" },
    { "__PTRDIFF_TYPE__",    "int" },
    { "__WCHAR_TYPE__",      "short" },
    { "__WINT_TYPE__",       "int" },
    { "__INTPTR_TYPE__",     "int" },
    { "__UINTPTR_TYPE__",    "unsigned int" },
    { "__INTMAX_TYPE__",     "long long int" },
    { "__UINTMAX_TYPE__",    "long long unsigned int" },
    { "__INT8_TYPE__",       "signed char" },
    { "__UINT8_TYPE__",      "unsigned char" },
    { "__INT16_TYPE__",      "short" },
    { "__UINT16_TYPE__",     "unsigned short" },
    { "__INT24_TYPE__",      "int" },
    { "__UINT24_TYPE__",     "unsigned int" },
    { "__INT32_TYPE__",      "long int" },
    { "__UINT32_TYPE__",     "long unsigned int" },
    { "__INT64_TYPE__",      "long long int" },
    { "__UINT64_TYPE__",     "long long unsigned int" },

    /* How wide each of them is, in bytes, and a char in bits. */
    { "__CHAR_BIT__",        "8" },
    { "__SIZEOF_SHORT__",    "2" },
    { "__SIZEOF_INT__",      "3" },
    { "__SIZEOF_LONG__",     "4" },
    { "__SIZEOF_LONG_LONG__", "8" },
    { "__SIZEOF_FLOAT__",    "4" },
    { "__SIZEOF_DOUBLE__",   "4" },
    { "__SIZEOF_LONG_DOUBLE__", "8" },
    { "__SIZEOF_POINTER__",  "3" },
    { "__SIZEOF_SIZE_T__",   "3" },
    { "__SIZEOF_PTRDIFF_T__", "3" },
    { "__SIZEOF_WCHAR_T__",  "2" },
    { "__SIZEOF_WINT_T__",   "3" },

    /* And the largest each will hold. */
    { "__SCHAR_MAX__",       "127" },
    { "__SHRT_MAX__",        "32767" },
    { "__INT_MAX__",         "8388607" },
    { "__LONG_MAX__",        "2147483647L" },
    { "__LONG_LONG_MAX__",   "9223372036854775807LL" },

    /* Which end the low byte is at. The three orders are named so that a
     * program can compare against them without knowing the numbers. */
    { "__ORDER_LITTLE_ENDIAN__", "1234" },
    { "__ORDER_BIG_ENDIAN__",    "4321" },
    { "__ORDER_PDP_ENDIAN__",    "3412" },
    { "__BYTE_ORDER__",      "__ORDER_LITTLE_ENDIAN__" }
};

void predefined_macros_init(void)
{
    int i;

    for (i = 0; i < (int) (sizeof predefined_macros
                           / sizeof *predefined_macros); i++) {
        const char *name = predefined_macros[i].name;
        const char *text = predefined_macros[i].text;

        macro_define(name_intern(name, (int) strlen(name)),
                     text, (int) strlen(text), NULL, -1, 0);
    }
}

/* A macro given on the command line, as though the file had begun with the
 * directive that says it.
 *
 * Written out as `#define` would have read it and handed to the same code,
 * rather than built from the parts here: a name given on the command line
 * is a macro like any other, down to taking parameters, and two ways of
 * making one is one more than is needed. The text is pushed as a window of
 * its own -- the lexer already reads a macro's expansion that way -- and
 * popped when the directive has had it.
 *
 * `-DNAME` is `-DNAME=1`. `-D'NAME(a,b)=...'` is a macro with parameters,
 * because the bracket follows the name with nothing between them, which is
 * the same rule the directive has. */
void lex_define(const char *arg)
{
    size_t n = strlen(arg);
    const char *eq = strchr(arg, '=');
    char *text = malloc(n + 4);

    if (!is_alpha((unsigned char) arg[0]))
        acc_error("-D needs a name, and '%s' does not begin with one", arg);
    opt_fold('D', arg);
    if (!text)
        acc_error("out of memory for a -D");
    if (eq) {
        memcpy(text, arg, (size_t) (eq - arg));
        text[eq - arg] = ' ';
        strcpy(text + (eq - arg) + 1, eq + 1);
    } else {
        memcpy(text, arg, n);
        strcpy(text + n, " 1");
    }
    strcat(text, "\n");

    push_owned_text(NAME_NONE, text);
    do_define();
    pop_source();
}

/* And one taken away again, the same way. `-U` of a name nothing defined
 * is allowed, as `#undef` of one is. */
void lex_undefine(const char *arg)
{
    char *text = malloc(strlen(arg) + 2);

    if (!is_alpha((unsigned char) arg[0]))
        acc_error("-U needs a name, and '%s' does not begin with one", arg);
    opt_fold('U', arg);
    if (!text)
        acc_error("out of memory for a -U");
    strcpy(text, arg);
    strcat(text, "\n");

    push_owned_text(NAME_NONE, text);
    do_undef();
    pop_source();
}
