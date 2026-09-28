/*
 * Directives: a logical line, #include and #pragma once, the #if
 * expression, the conditionals, #define and #undef, #error, #line and
 * #pragma.
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

Cond conds[COND_MAX];
int  nconds;

/* ------------------------------------------------------------------ */
/* directives                                                          */

static void directive(void);
static int  directive_name(char *buf, int cap);
static NameRef directive_target(const char *what);
static void skip_blanks(void);
static void rest_of_line(void);

/* Past blanks and the comments among them, to what really follows on the
 * line: a comment is one space by the time directives are read (C99
 * 5.1.1.2), so `#include <stdio.h> // printf` has nothing after its name.
 * A block comment that runs on past the line's end is left where it is. */
static const char *blanks_and_comments(const char *p)
{
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\r')
            p++;
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n')
                p++;

            return p;
        }
        if (p[0] == '/' && p[1] == '*') {
            const char *end = p + 2;

            while (*end && *end != '\n' && !(end[0] == '*' && end[1] == '/'))
                end++;
            if (end[0] != '*')
                return p;
            p = end + 2;

            continue;
        }

        return p;
    }
}

/* Space that is not a line's end: a directive lives on one line, so the
 * newline that ends it is what stops the scan rather than something to be
 * skipped over. */
static void skip_blanks(void)
{
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\r')
        cursor++;
}

/* What is left of a directive's line, gathered rather than pointed at: a
 * macro's replacement text, or the condition of an #if.
 *
 * Two reasons it is copied. A line ending in a backslash is joined to the
 * one after it, which is how a macro of more than one line is written, and
 * the join has to take the backslash and the newline out of the text. And
 * the window may be refilled in the middle of a long one, which moves what
 * is left of it to the front of the buffer -- so a pointer to where the text
 * started would no longer be pointing at it.
 *
 * The buffer is grown and kept, since a file full of macros would otherwise
 * ask for one per macro. What is in it lasts until the next directive, so
 * macro_define copies what it is given. */
static char *body;
static int   body_cap;

static int logical_line(void)
{
    int len = 0;

    for (;;) {
        while (*cursor && *cursor != '\n') {
            int comment = cursor[0] == '/'
                          && (cursor[1] == '/' || cursor[1] == '*');

            if (len == body_cap) {
                body_cap = body_cap ? body_cap * 2 : 256;
                body = realloc(body, (size_t) body_cap);
                if (!body)
                    acc_error("out of memory for a macro");
            }

            /* A comment is one space. C takes them out before it reads
             * directives, so one that opens on a directive's line and closes
             * on a later one is still part of that directive -- and what
             * follows the close is still part of it too. Read as text, the
             * body kept the comment's opening and the lines under it were
             * compiled as though they were code, which is what a define with
             * a comment laid out over two lines did to acc's own code generator. */
            if (comment) {
                skip_comment();
                body[len++] = ' ';

                continue;
            }
            body[len++] = *cursor++;
        }

        /* A backslash last on the line, bar the carriage return a file
         * written on another machine leaves there, joins this line to the
         * next.
         *
         * Both characters go, rather than becoming a space: C deletes them
         * before anything is tokenised, so a name split over two lines is
         * one name. `#define AB\<newline>CD 42` defines ABCD, and a space
         * in the join would have defined AB and left CD after it. */
        if (*cursor == '\n') {
            int end = len;

            if (end && body[end - 1] == '\r')
                end--;
            if (end && body[end - 1] == '\\') {
                len = end - 1;
                cursor++;
                line++;

                continue;
            }
        }
        if (*cursor || !refill())
            return len;
    }
}

/* The rest of the directive's line, thrown away. The newline is left for
 * skip_space, which is what counts the lines. A block comment is one space,
 * so one that runs past the line's end takes the directive with it to where
 * it closes (pragma-pack-3); a string is passed over whole, so that what
 * opens a comment inside one is not taken for one. */
static void rest_of_line(void)
{
    int in_comment = 0, quote = 0;

    for (;;) {
        while (*cursor && (*cursor != '\n' || in_comment)) {
            if (in_comment) {
                if (cursor[0] == '*' && cursor[1] == '/') {
                    in_comment = 0;
                    cursor++;
                } else if (*cursor == '\n') {
                    line++;
                }
            } else if (quote) {
                if (*cursor == '\\' && cursor[1] && cursor[1] != '\n')
                    cursor++;
                else if (*cursor == quote)
                    quote = 0;
            } else if (*cursor == '"' || *cursor == '\'') {
                quote = *cursor;
            } else if (cursor[0] == '/' && cursor[1] == '*') {
                in_comment = 1;
                cursor++;
            } else if (cursor[0] == '/' && cursor[1] == '/') {
                while (*cursor && *cursor != '\n')
                    cursor++;
                break;
            }
            cursor++;
        }
        if (*cursor || !refill())
            return;
    }
}

/* The name after the '#', into `buf`. Returns its length, which is zero for
 * a '#' on a line of its own -- which C allows and which does nothing. */
static int directive_name(char *buf, int cap)
{
    int n = 0;

    while (is_alnum((unsigned char) *cursor) && n < cap - 1)
        buf[n++] = *cursor++;
    buf[n] = '\0';

    return n;
}

/* The file named by an `#include`, into `buf`. Returns the quote character,
 * '"' or '<', so the caller knows where to look for it. */
static int include_name(char *buf, int cap)
{
    int close, n = 0;
    int open_ch = (unsigned char) *cursor;

    if (open_ch != '"' && open_ch != '<')
        acc_error_at(line, "an #include needs \"a file\" or <a file>");
    close = open_ch == '"' ? '"' : '>';
    cursor++;

    while (*cursor && *cursor != close && *cursor != '\n') {
        if (n == cap - 1)
            acc_error_at(line, "the file name in an #include is too long");
        buf[n++] = *cursor++;
    }
    if (*cursor != close)
        acc_error_at(line, "the file name in an #include is not closed");
    cursor++;
    buf[n] = '\0';
    if (!n)
        acc_error_at(line, "an #include needs a file name");

    return open_ch;
}

/* dir + "/" + name, or just name when there is no directory. Returns 0 when
 * the two together do not fit. */
static int join_path(char *out, int cap, const char *dir, int dirlen,
                     const char *name)
{
    int n = (int) strlen(name);

    if (dirlen + (dirlen ? 1 : 0) + n >= cap)
        return 0;
    if (dirlen) {
        memcpy(out, dir, (size_t) dirlen);
        out[dirlen] = '/';
        strcpy(out + dirlen + 1, name);
    } else {
        strcpy(out, name);
    }

    return 1;
}

static int readable(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f)
        return 0;
    fclose(f);

    return 1;
}

/* Where a named file is, or null. A quoted name is looked for beside the
 * file that asked for it first, which is what makes a header next to its
 * source findable without an -I; an angled one is not. Both then go through
 * the -I directories in the order they were given. */
static const char *find_include(int how, const char *name, char *buf, int cap)
{
    int i;

    /* An absolute path, or one the working directory already answers. */
    if (name[0] == '/')
        return readable(name) ? name : NULL;

    if (how == '"') {
        const char *slash = NULL, *p;

        for (p = src_path; *p; p++)
            if (*p == '/')
                slash = p;
        if (join_path(buf, cap, src_path, slash ? (int) (slash - src_path) : 0,
                      name)
            && readable(buf))
            return buf;
    }

    for (i = 0; i != ninclude_dirs; i++)
        if (join_path(buf, cap, include_dirs[i],
                      (int) strlen(include_dirs[i]), name)
            && readable(buf))
            return buf;

    return NULL;
}

/* The files a `#pragma once` has been seen in, by the path they were opened
 * under.
 *
 * Not C, but every compiler has it and headers are written expecting it: a
 * header with neither this nor a guard is read again on every include, and
 * what that does to a program is redefine everything in it. The path is the
 * one the include resolved to rather than whatever `#line` has since called
 * the file, so that a generated header cannot rename itself out of the set.
 *
 * Two names for the same file are two entries, which is the answer every
 * compiler that compares paths gives. A header found down two different -I
 * directories is read twice, and a guard is what covers that; the two
 * together are the usual belt and braces. */
static char **once_seen;
static int    nonce, once_cap;

static int once_has(const char *path)
{
    int i;

    for (i = 0; i != nonce; i++)
        if (strcmp(once_seen[i], path) == 0)
            return 1;

    return 0;
}

void once_add(const char *path)
{
    char *keep;

    if (!path || once_has(path))
        return;
    if (nonce == once_cap) {
        once_cap = once_cap ? once_cap * 2 : 8;
        once_seen = realloc(once_seen, (size_t) once_cap * sizeof *once_seen);
        if (!once_seen)
            acc_error("out of memory for the files read once");
    }
    keep = malloc(strlen(path) + 1);
    if (!keep)
        acc_error("out of memory for the files read once");
    strcpy(keep, path);
    once_seen[nonce++] = keep;
}

/* #include, given room for the name and for the path it is found at. The
 * room is do_include's: 384 bytes of it, inlined into directives, made
 * that frame 446 bytes, and every local past the 128 an (ix+d) reaches was
 * a computed address -- 41 of them. In do_include alone it did the same to
 * this function's 26. So do_include is the buffers and nothing else, and
 * the work is here, where every local is in reach. */
#define INCLUDE_NAME_MAX 128
#define INCLUDE_PATH_MAX 256

__attribute__((noinline))
static void include_in(char *name, char *buf)
{
    const char *found;
    int how;

    skip_blanks();
    how = include_name(name, INCLUDE_NAME_MAX);
    cursor = (char *) blanks_and_comments(cursor);
    if (*cursor && *cursor != '\n')
        acc_error_at(line, "an #include takes one file name and nothing else");

    found = find_include(how, name, buf, INCLUDE_PATH_MAX);
    if (!found)
        acc_error_at(line, "cannot find '%s'", name);

    /* The rest of this file's line goes first: the push swaps the window
     * out, and what is left of the line has to be behind the cursor when it
     * comes back. */
    rest_of_line();
    if (once_has(found))
        return;                         /* it said `#pragma once` already */
    push_source(found);
}

__attribute__((noinline))
static void do_include(void)
{
    char name[INCLUDE_NAME_MAX], buf[INCLUDE_PATH_MAX];

    include_in(name, buf);
}

/* ------------------------------------------------------------------ */
/* conditionals                                                        */

/* ------------------------------------------------------------------ */
/* #if                                                                 */

/* The line an `#if` is given, with the macros in it put back as their text
 * and `defined X` answered, built into a buffer and then read as an
 * expression.
 *
 * Done on the text rather than through the lexer because the lexer is in
 * the middle of a directive: asking it for the next token would run it off
 * the end of the line, and putting it back afterwards is more machinery
 * than copying the line is. */
static char *if_text;
static int   if_len, if_cap;

static void if_put(const char *text, int len)
{
    if (if_len + len > if_cap) {
        int want = if_cap ? if_cap * 2 : 512;

        while (want < if_len + len)
            want *= 2;
        if_text = realloc(if_text, (size_t) want);
        if (!if_text)
            acc_error("out of memory expanding a macro");
        if_cap = want;
    }
    memcpy(if_text + if_len, text, (size_t) len);
    if_len += len;
}

/* The names being expanded, so that one does not expand inside itself. */
static NameRef if_active[INCLUDE_MAX];
static int     if_depth;

static void if_expand(const char *text, int len);

/* The arguments of a call written in text rather than read from the file:
 * what a macro's expansion holds, and what an #if's condition holds. The
 * nesting rules are the same as at a call the lexer reads. */
static char **collect_args_text(Macro *m, const char **at, const char *end,
                                int *out_argc)
{
    const char *p = *at;
    char **argv;
    Buf    arg;
    int    argc = 0, depth = 0, i, cap = ARGS_FIRST;

    argv = calloc(ARGS_FIRST, sizeof *argv);
    if (!argv)
        acc_error("out of memory for a macro's arguments");
    arg.text = NULL; arg.len = 0; arg.cap = 0;

    while (p < end && is_space((unsigned char) *p))
        p++;
    p++;                                /* the '(' */

    for (;;) {
        int c;

        if (p == end)
            acc_error_at(line, "a macro's arguments are not closed");
        c = (unsigned char) *p++;

        if (c == '(' || c == '[') {
            depth++;
        } else if (c == ')' && depth == 0) {
            break;
        } else if (c == ')' || c == ']') {
            depth--;
        } else if (c == ',' && depth == 0
                   && !(m->variadic && argc == m->nparams)) {
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
            while (p < end) {
                c = (unsigned char) *p++;
                buf_putc(&arg, c);
                if (c == '\\' && p < end) {
                    buf_putc(&arg, (unsigned char) *p++);

                    continue;
                }
                if (c == quote)
                    break;
            }

            continue;
        }

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
    if (m->nparams == 0 && argc == 1 && !*argv[0]) {
        free(argv[0]);                  /* as above: nothing, not one empty */
        argv[0] = NULL;
        argc = 0;
    }
    for (i = 0; i != argc; i++) {
        char *a = argv[i];
        int   n = (int) strlen(a);

        while (n && a[n - 1] == ' ')
            a[--n] = '\0';
        if (*a == ' ')
            memmove(a, a + 1, strlen(a));
    }

    *at = p;
    *out_argc = argc;

    return argv;
}

/* A name in text being expanded: put back as what it stands for, or left
 * alone. `at` is the name; `after` is what follows it, which is what says
 * whether a macro with parameters is being used or merely mentioned.
 * Returns where to carry on from. */
static const char *if_name(const char *at, int len, const char *after,
                           const char *end)
{
    NameRef name = name_intern(at, len);
    Macro  *m;
    int     i;

    /* One of the names the compiler defines. They are keywords rather than
     * macros, so the table below has never heard of them, and without this
     * `#if __STDC__` would read as the zero an undefined name stands for.
     * The two that are strings are put back as strings, which is not
     * something an #if can do anything with -- and saying so where the
     * expression is read is a better answer than a silent zero. */
    if (predefined_name(name)) {
        char num[24];

        switch ((unsigned char) name_arena[name - 3]) {
        case TK_LINE:
            snprintf(num, sizeof num, "%d", line);
            if_put(num, (int) strlen(num));
            break;
        case TK_STDC:
            if_put("1", 1);
            break;
        case TK_STDC_VERSION:
            if_put("199901L", 7);
            break;
        case TK_STDC_HOSTED:
            if_put("0", 1);
            break;
        case TK_DATE: if_put("\"", 1);
                      if_put(date_text, (int) strlen(date_text));
                      if_put("\"", 1); break;
        case TK_TIME: if_put("\"", 1);
                      if_put(time_text, (int) strlen(time_text));
                      if_put("\"", 1); break;
        default: {
            const char *f = src_path ? src_path : "";

            if_put("\"", 1);
            if_put(f, (int) strlen(f));
            if_put("\"", 1);
            break;
        }
        }

        return after;
    }

    for (i = 0; i < if_depth; i++)
        if (if_active[i] == name) {
            if_put(at, len);            /* already being expanded */

            return after;
        }

    m = macro_find(name);
    if (!m) {
        if_put(at, len);

        return after;
    }
    if (if_depth == INCLUDE_MAX)
        acc_error_at(line, "macros expanded more than %d deep", INCLUDE_MAX);

    if (m->nparams >= 0) {
        const char *p = after;
        char **argv;
        char  *built;
        int    argc;

        while (p < end && is_space((unsigned char) *p))
            p++;
        if (p == end || *p != '(') {
            if_put(at, len);            /* mentioned, not used */

            return after;
        }

        p = after;
        argv = collect_args_text(m, &p, end, &argc);
        if (argc < m->nparams || (argc > m->nparams && !m->variadic))
            acc_error_at(line, "'%s' takes %d argument%s, not %d",
                         name_text(name), m->nparams,
                         m->nparams == 1 ? "" : "s", argc);
        built = build_expansion(m, argv, argc);
        free_args(argv, argc);

        if_active[if_depth++] = name;
        if_expand(built, (int) strlen(built));
        if_depth--;
        free(built);

        return p;
    }

    if_active[if_depth++] = name;
    if_expand(m->text, (int) strlen(m->text));
    if_depth--;

    return after;
}

/* `defined X` and `defined(X)`, which are answered before anything is
 * expanded: the name is the one that was written, not what it stands for. */
static const char *if_defined(const char *p, const char *end)
{
    const char *start;
    int parens = 0;

    while (p < end && (*p == ' ' || *p == '\t'))
        p++;
    if (p < end && *p == '(') {
        parens = 1;
        p++;
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;
    }
    start = p;
    while (p < end && is_alnum((unsigned char) *p))
        p++;
    if (p == start)
        acc_error_at(line, "'defined' needs a name");

    {
        NameRef name = name_intern(start, (int) (p - start));

        if_put(macro_find(name) || predefined_name(name) ? "1" : "0", 1);
    }

    if (parens) {
        while (p < end && (*p == ' ' || *p == '\t'))
            p++;
        if (p == end || *p != ')')
            acc_error_at(line, "'defined(' needs a ')'");
        p++;
    }

    return p;
}

/* One stretch of text into the buffer, with its names dealt with. */
static void if_expand(const char *text, int len)
{
    const char *p = text, *end = text + len;

    while (p < end) {
        const char *start;

        if (!is_alpha((unsigned char) *p)) {
            /* A string or a character constant goes over whole, so that a
             * name inside one is not a name. */
            if (*p == '"' || *p == '\'') {
                int quote = *p;

                start = p++;
                while (p < end && *p != quote) {
                    if (*p == '\\' && p + 1 < end)
                        p++;
                    p++;
                }
                if (p < end)
                    p++;
                if_put(start, (int) (p - start));

                continue;
            }
            if_put(p, 1);
            p++;

            continue;
        }


        start = p;
        while (p < end && is_alnum((unsigned char) *p))
            p++;

        if (p - start == 7 && !memcmp(start, "defined", 7)) {
            p = if_defined(p, end);

            continue;
        }
        p = if_name(start, (int) (p - start), p, end);
    }
}

/* ------------------------------------------------------------------ */

/* The expression, read from the buffer the expansion built.
 *
 * C says an #if is worked out in the widest integers there are, which here
 * is a long long, and that a name left over is zero -- `#if FOO` on a name
 * nobody defined is false rather than a mistake. */
static const char *ep;

static long long if_ternary(void);

/* A hex digit's value, or -1. float.c has one of these, but it is static
 * there and this is the only other place that wants one. */
static int if_digit(int c, int base)
{
    int v;

    if (c >= '0' && c <= '9')
        v = c - '0';
    else if (c >= 'a' && c <= 'f')
        v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        v = c - 'A' + 10;
    else
        return -1;

    return v < base ? v : -1;
}

static void ep_blanks(void)
{
    while (*ep == ' ' || *ep == '\t' || *ep == '\r')
        ep++;
}

/* Whether the operator `op` is next, stepping over it if so. Two-character
 * operators are asked for before the one-character ones they start with. */
static int ep_op(const char *op)
{
    int n = (int) strlen(op);

    ep_blanks();
    if (memcmp(ep, op, (size_t) n) != 0)
        return 0;

    /* `&` must not match the front of `&&`, nor `<` that of `<<` or `<=`. */
    if (n == 1 && (ep[1] == ep[0] || ep[1] == '=')
        && (*op == '&' || *op == '|' || *op == '<' || *op == '>'
            || *op == '=' || *op == '!'))
        return 0;
    ep += n;

    return 1;
}

/* How deep inside an operand C says is not evaluated: the right of a `&&`
 * whose left was false, of a `||` whose left was true, and the branch of a
 * `?:` that was not taken. The text still has to be read, because it has to
 * be stepped over, but nothing in it may be complained about -- `#if 0 &&
 * 1/0` is a well-formed condition that is false, and a program that guards
 * a division that way is entitled to have it not diagnosed. */
static int if_dead;

static long long if_primary(void)
{
    long long v;

    ep_blanks();
    if (*ep == '(') {
        ep++;
        v = if_ternary();
        ep_blanks();
        if (*ep != ')')
            acc_error_at(line, "the expression in an #if needs a ')'");
        ep++;

        return v;
    }
    if (*ep == '\'' || (ep[0] == 'L' && ep[1] == '\'')) {
        /* A character constant, whose value is what the byte is. The
         * escapes are the ones the lexer proper takes, numbers and all:
         * `#if '\\x41' == 'A'` is a condition a program may reasonably
         * write, and one that reads only `\\0` cannot answer it.
         *
         * An L in front makes it wide, as in the program: a wchar_t, which
         * is a short, an escape as wide as that, and a character of the
         * source read as the UTF-8 it is. */
        int wide = *ep == 'L';

        ep += wide + 1;
        if (*ep == '\\') {
            ep++;
            switch (*ep) {
            case 'n':  v = '\n'; ep++; break;
            case 't':  v = '\t'; ep++; break;
            case 'r':  v = '\r'; ep++; break;
            case 'a':  v = '\a'; ep++; break;
            case 'b':  v = '\b'; ep++; break;
            case 'f':  v = '\f'; ep++; break;
            case 'v':  v = '\v'; ep++; break;
            case 'e':  v = 27;   ep++; break;
            case '\\': v = '\\'; ep++; break;
            case '\'': v = '\''; ep++; break;
            case '"':  v = '"';  ep++; break;
            case '?':  v = '?';  ep++; break;
            case 'x': {
                int d;

                ep++;
                if (if_digit((unsigned char) *ep, 16) < 0)
                    acc_error_at(line, "'\\x' in an #if needs a hex digit");
                v = 0;
                while ((d = if_digit((unsigned char) *ep, 16)) >= 0) {
                    v = v * 16 + d;
                    ep++;
                }
                v = wide ? (short) v : (signed char) v;
                break;
            }
            default:
                if (if_digit((unsigned char) *ep, 8) >= 0) {
                    int n = 0, d;

                    v = 0;
                    while (n < 3
                           && (d = if_digit((unsigned char) *ep, 8)) >= 0) {
                        v = v * 8 + d;
                        ep++;
                        n++;
                    }
                    v = wide ? (short) v : (signed char) v;
                } else {
                    v = (unsigned char) *ep;
                    ep++;
                }
                break;
            }
        } else if (wide && (unsigned char) *ep >= 0xc0) {
            int c = (unsigned char) *ep++;
            int more = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : 1;

            v = c & (0x3f >> more);
            while (more-- && ((unsigned char) *ep & 0xc0) == 0x80)
                v = v << 6 | (*ep++ & 0x3f);
            if (v > 0xffff)
                acc_error_at(line, "a wide character constant past 0xffff "
                                   "does not fit in a wchar_t");
            v = (short) v;
        } else {
            v = wide ? (unsigned char) *ep++ : (signed char) *ep++;
        }
        if (*ep != '\'')
            acc_error_at(line, "a character constant in an #if is not closed");
        ep++;

        return v;
    }
    if (is_digit((unsigned char) *ep)) {
        int base = 10;

        if (ep[0] == '0' && (ep[1] == 'x' || ep[1] == 'X')) {
            base = 16;
            ep += 2;
        } else if (ep[0] == '0') {
            base = 8;
        }
        v = 0;
        for (;;) {
            int d = if_digit((unsigned char) *ep, base);

            if (d < 0)
                break;
            v = v * base + d;
            ep++;
        }
        while (*ep == 'u' || *ep == 'U' || *ep == 'l' || *ep == 'L')
            ep++;

        return v;
    }
    if (is_alpha((unsigned char) *ep)) {
        /* A name nothing defined, which C says is zero. */
        while (is_alnum((unsigned char) *ep))
            ep++;

        return 0;
    }
    if (!*ep)
        acc_error_at(line, "the expression in an #if stops too soon");

    acc_error_at(line, "'%c' has no meaning in an #if", *ep);

    return 0;
}

static long long if_unary(void)
{
    ep_blanks();
    if (ep_op("!"))
        return !if_unary();
    if (ep_op("~"))
        return ~if_unary();
    if (ep_op("-"))
        return -if_unary();
    if (ep_op("+"))
        return if_unary();

    return if_primary();
}

static long long if_mul(void)
{
    long long v = if_unary();

    for (;;) {
        long long r;

        if (ep_op("*")) {
            v = v * if_unary();
        } else if (ep_op("/")) {
            r = if_unary();
            if (!r) {
                if (!if_dead)
                    acc_error_at(line, "a division by zero in an #if");
                r = 1;
            }
            v = v / r;
        } else if (ep_op("%")) {
            r = if_unary();
            if (!r) {
                if (!if_dead)
                    acc_error_at(line, "a division by zero in an #if");
                r = 1;
            }
            v = v % r;
        } else {
            return v;
        }
    }
}

static long long if_add(void)
{
    long long v = if_mul();

    for (;;) {
        if (ep_op("+"))
            v = v + if_mul();
        else if (ep_op("-"))
            v = v - if_mul();
        else
            return v;
    }
}

static long long if_shift(void)
{
    long long v = if_add();

    for (;;) {
        if (ep_op("<<"))
            v = v << if_add();
        else if (ep_op(">>"))
            v = v >> if_add();
        else
            return v;
    }
}

static long long if_relational(void)
{
    long long v = if_shift();

    for (;;) {
        if (ep_op("<="))
            v = v <= if_shift();
        else if (ep_op(">="))
            v = v >= if_shift();
        else if (ep_op("<"))
            v = v < if_shift();
        else if (ep_op(">"))
            v = v > if_shift();
        else
            return v;
    }
}

static long long if_equality(void)
{
    long long v = if_relational();

    for (;;) {
        if (ep_op("=="))
            v = v == if_relational();
        else if (ep_op("!="))
            v = v != if_relational();
        else
            return v;
    }
}

static long long if_bitand(void)
{
    long long v = if_equality();

    while (ep_op("&"))
        v = v & if_equality();

    return v;
}

static long long if_bitxor(void)
{
    long long v = if_bitand();

    while (ep_op("^"))
        v = v ^ if_bitand();

    return v;
}

static long long if_bitor(void)
{
    long long v = if_bitxor();

    while (ep_op("|"))
        v = v | if_bitxor();

    return v;
}

static long long if_and(void)
{
    long long v = if_bitor();

    /* The right is read whether or not the left decided it -- it is text
     * that has to be stepped over either way -- but it is not evaluated
     * when the left settled the answer. See if_dead. */
    while (ep_op("&&")) {
        long long r;

        if_dead += !v;
        r = if_bitor();
        if_dead -= !v;
        v = v && r;
    }

    return v;
}

static long long if_or(void)
{
    long long v = if_and();

    while (ep_op("||")) {
        long long r;

        if_dead += !!v;
        r = if_and();
        if_dead -= !!v;
        v = v || r;
    }

    return v;
}

static long long if_ternary(void)
{
    long long v = if_or();

    if (!ep_op("?"))
        return v;
    {
        long long yes, no;

        if_dead += !v;
        yes = if_ternary();
        if_dead -= !v;
        if (!ep_op(":"))
            acc_error_at(line, "the '?' in an #if needs a ':'");
        if_dead += !!v;
        no = if_ternary();
        if_dead -= !!v;

        return v ? yes : no;
    }
}

/* Text with the macros in it put back, into a buffer. The same walk an
 * #if's condition goes through, and what a macro's argument goes through
 * before it is put in where the parameter was.
 *
 * The buffer it builds in is one shared static, so what is in it is saved
 * and put back: expanding an argument can reach a macro whose own argument
 * has to be expanded, and the inner one would otherwise write over the
 * outer one's work. */
void expand_text_into(Buf *out, const char *text)
{
    char *saved = NULL;
    int   saved_len = if_len, saved_depth = if_depth, i;
    NameRef saved_active[INCLUDE_MAX];

    if (saved_len) {
        saved = malloc((size_t) saved_len);
        if (!saved)
            acc_error("out of memory expanding a macro");
        memcpy(saved, if_text, (size_t) saved_len);
    }
    for (i = 0; i != saved_depth; i++)
        saved_active[i] = if_active[i];

    if_len = 0;
    if_depth = 0;
    if_expand(text, (int) strlen(text));
    if_put("", 1);
    buf_put(out, if_text, if_len - 1);

    if (saved) {
        memcpy(if_text, saved, (size_t) saved_len);
        free(saved);
    }
    if_len = saved_len;
    if_depth = saved_depth;
    for (i = 0; i != saved_depth; i++)
        if_active[i] = saved_active[i];
}

/* The condition of an #if or an #elif: what is left of the line, expanded,
 * and then read. */
static int if_condition(void)
{
    long long v;
    int len;

    skip_blanks();
    len = logical_line();

    if_len = 0;
    if_depth = 0;
    if_expand(body ? body : "", len);
    if_put("", 1);                      /* the terminator */

    ep = if_text;
    v = if_ternary();
    ep_blanks();
    if (*ep)
        acc_error_at(line, "'%c' is left over at the end of an #if", *ep);

    return v != 0;
}

/* One `#if` and its `#else`, innermost last. `taken` is whether any branch
 * of this group has been used, which is what makes the `#else` of a group
 * whose `#ifdef` was true a group to skip. */
/* What ended a skipped group. */
enum { GROUP_ELSE, GROUP_ELIF, GROUP_ENDIF };

/* Past a group that is not taken, to the directive that ends it.
 *
 * The text inside is not lexed: a group under an `#if 0` is only required
 * to be made of preprocessing tokens, and in practice holds prose, half a
 * function, or an apostrophe that would close nothing. So this reads lines,
 * not tokens, and looks at nothing but the directives.
 *
 * Block comments are the exception, because one can legitimately run over
 * the `#endif` that would otherwise end the group. */
static int skip_group(void)
{
    int nest = 0, in_comment = 0, opened = line;
    char name[32];

    for (;;) {
        /* At the start of a line. */
        if (!in_comment) {
            skip_blanks();
            if (hash_at(cursor)) {
                cursor += hash_at(cursor);
                skip_blanks();
                directive_name(name, (int) sizeof name);

                /* Only the conditionals are looked at in here: an
                 * `#error` under an `#if 0` is one the program meant not
                 * to say. */
                if (!strcmp(name, "if") || !strcmp(name, "ifdef")
                    || !strcmp(name, "ifndef")) {
                    nest++;
                } else if (!strcmp(name, "endif")) {
                    if (!nest)
                        return GROUP_ENDIF;
                    nest--;
                } else if (!nest && !strcmp(name, "else")) {
                    return GROUP_ELSE;
                } else if (!nest && !strcmp(name, "elif")) {
                    return GROUP_ELIF;
                }
            }
        }

        /* The rest of the line, watching for a comment that opens on it and
         * for the end of one that opened earlier. */
        for (;;) {
            while (*cursor && *cursor != '\n') {
                if (in_comment) {
                    if (cursor[0] == '*' && cursor[1] == '/') {
                        in_comment = 0;
                        cursor += 2;

                        continue;
                    }
                } else if (cursor[0] == '/' && cursor[1] == '*') {
                    in_comment = 1;
                    cursor += 2;

                    continue;
                } else if (cursor[0] == '/' && cursor[1] == '/') {
                    break;      /* the rest of the line is a comment */
                }
                cursor++;
            }
            while (*cursor && *cursor != '\n')
                cursor++;
            if (*cursor)
                break;
            if (!refill())
                acc_error_at(opened, "#if without #endif");
        }
        cursor++;               /* the newline */
        line++;
    }
}

static void cond_push(int taken)
{
    if (nconds == COND_MAX)
        acc_error_at(line, "conditionals nested more than %d deep", COND_MAX);
    conds[nconds].taken = (unsigned char) (taken != 0);
    conds[nconds].seen_else = 0;
    conds[nconds].line = line;
    nconds++;
}

/* A group whose condition was false is skipped, and whatever ends it is
 * handled here rather than by the directive loop: the `#else` of a group
 * that was skipped starts a group that is taken. */
static void cond_skip_until_taken(void)
{
    for (;;) {
        int end = skip_group();

        if (end == GROUP_ENDIF) {
            nconds--;

            return;
        }
        if (end == GROUP_ELSE) {
            if (conds[nconds - 1].seen_else)
                acc_error_at(line, "#else after #else");
            conds[nconds - 1].seen_else = 1;
            if (!conds[nconds - 1].taken) {
                conds[nconds - 1].taken = 1;

                return;         /* this branch is the one that runs */
            }

            continue;           /* a branch was taken already: skip on */
        }

        /* #elif. Its condition only has to be worked out when no branch of
         * this group has run yet; after one has, the rest are skipped
         * whatever they say. */
        if (conds[nconds - 1].seen_else)
            acc_error_at(line, "#elif after #else");
        if (conds[nconds - 1].taken)
            continue;
        if (if_condition()) {
            conds[nconds - 1].taken = 1;

            return;
        }
    }
}

static void do_if(void)
{
    if (if_condition()) {
        cond_push(1);

        return;
    }
    cond_push(0);
    cond_skip_until_taken();
}

/* An #elif reached by reading rather than by skipping: the branch before it
 * ran, so this one does not whatever it says. */
static void do_elif(void)
{
    if (!nconds)
        acc_error_at(line, "#elif without #if");
    if (conds[nconds - 1].seen_else)
        acc_error_at(line, "#elif after #else");
    rest_of_line();
    cond_skip_until_taken();
}

static void do_ifdef(int want)
{
    NameRef name;

    skip_blanks();
    name = directive_target(want ? "ifdef" : "ifndef");
    rest_of_line();

    if ((macro_find(name) != NULL || predefined_name(name)) == want) {
        cond_push(1);

        return;
    }
    cond_push(0);
    cond_skip_until_taken();
}

static void do_else(void)
{
    if (!nconds)
        acc_error_at(line, "#else without #if");
    if (conds[nconds - 1].seen_else)
        acc_error_at(line, "#else after #else");
    conds[nconds - 1].seen_else = 1;
    rest_of_line();

    /* Getting here means the branch before this one ran, so this one does
     * not: skip to the #endif. */
    cond_skip_until_taken();
}

static void do_endif(void)
{
    if (!nconds)
        acc_error_at(line, "#endif without #if");
    nconds--;
    rest_of_line();
}

/* At the end of the outermost file: a conditional left open is a mistake
 * that would otherwise be silent. */
void lex_end(void)
{
    if (nconds)
        acc_error_at(conds[nconds - 1].line, "#if without #endif");
}

/* The name a #define or an #undef is about. Interned, so that what the
 * lexer will hand back for the same spelling is the same reference. */
static NameRef directive_target(const char *what)
{
    const char *start = cursor;

    if (!is_alpha((unsigned char) *cursor))
        acc_error_at(line, "#%s needs a name", what);
    while (is_alnum((unsigned char) *cursor))
        cursor++;

    return name_intern(start, (int) (cursor - start));
}

void do_define(void)
{
    NameRef name;
    NameRef *params = NULL;
    int nparams = -1, variadic = 0, pcap;
    int len;

    skip_blanks();
    name = directive_target("define");
    if (predefined_name(name))
        acc_error_at(line, "'%s' is the compiler's to define",
                     name_text(name));

    /* `#define f(x)` is a macro with parameters, which is a different thing
     * from a name standing for some text: the parenthesis has to follow the
     * name with nothing between it and the name to mean that, which is why
     * this is asked before any blanks are skipped. */
    if (*cursor == '(') {
        cursor++;
        pcap = 8;                       /* grown as a macro takes more */
        params = malloc((size_t) pcap * sizeof *params);
        if (!params)
            acc_error("out of memory for a macro's parameters");
        nparams = 0;

        skip_blanks();
        if (*cursor == ')') {
            cursor++;                   /* `f()`, which takes one empty one */
        } else {
            for (;;) {
                skip_blanks();
                if (cursor[0] == '.' && cursor[1] == '.' && cursor[2] == '.') {
                    cursor += 3;
                    variadic = 1;
                    skip_blanks();
                    break;
                }
                if (nparams == PARAMS_MAX)
                    acc_error_at(line, "a macro takes at most %d parameters",
                                 PARAMS_MAX);
                if (nparams == pcap) {
                    pcap *= 2;
                    params = realloc(params, (size_t) pcap * sizeof *params);
                    if (!params)
                        acc_error("out of memory for a macro's parameters");
                }
                params[nparams++] = directive_target("define");
                skip_blanks();
                if (*cursor != ',')
                    break;
                cursor++;
            }
            if (*cursor != ')')
                acc_error_at(line, "the parameters of a macro need a ')'");
            cursor++;
        }
    }

    skip_blanks();
    len = logical_line();

    /* Blanks at either end are left in. Nothing reads the text but the
     * lexer, which skips them; trimming them would be work whose result
     * nothing can tell apart. When `#` arrives it will be able to -- what a
     * parameter stringifies to is spelled out -- and the trimming belongs
     * with it, where a test can show the difference. */
    macro_define(name, body, len, params, nparams, variadic);
}

void do_undef(void)
{
    NameRef name;

    skip_blanks();
    name = directive_target("undef");
    if (predefined_name(name))
        acc_error_at(line, "'%s' is the compiler's to define",
                     name_text(name));
    cursor = (char *) blanks_and_comments(cursor);
    if (*cursor && *cursor != '\n')
        acc_error_at(line, "#undef takes one name and nothing else");

    macro_undef(name);
    rest_of_line();
}

/* `#error` with whatever the program has to say about it, which is the rest
 * of the line as written. */
static void do_error(void)
{
    const char *start;

    skip_blanks();
    start = cursor;
    rest_of_line();

    acc_error_at(line, "#error %.*s", (int) (cursor - start), start);
}

/* `#pragma`. C says an unknown one is ignored, and acc knows two. `once`
 * says this file is to be read once however many times it is included.
 * `weak NAME` says a reference to NAME is not a reason to link it: if
 * nothing else wants it, it is not looked for, and without it the address
 * is zero -- which is how printf reaches the floating conversions only in a
 * program that has a float to print. */
static void do_pragma(void)
{
    char what[32];

    skip_blanks();
    if (directive_name(what, (int) sizeof what)) {
        if (!strcmp(what, "once")) {
            once_add(src_real);
        } else if (!strcmp(what, "weak")) {
            skip_blanks();
            name_set_weak(directive_target("pragma weak"), 1);
        }
    }
    rest_of_line();
}

/* `#line N` and `#line N "file"`: what the lines after this one are called,
 * which is what a program that generates C uses to point at what it was
 * generated from.
 *
 * The number is the line *after* this one, and the newline that ends this
 * directive is still to be counted, so what is stored is one less. */
/* Blanks in whichever text a #line is being read out of. */
static const char *line_blanks(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r')
        p++;

    return p;
}

static void do_line(void)
{
    const char *p;
    long n = 0;
    int digits = 0, expanded = 0;

    skip_blanks();

    /* A #line whose number is not written as one has its tokens put through
     * the macros first, which is what makes `#define WHERE 500` and then
     * `#line WHERE` mean line 500. The common form costs nothing for it:
     * a digit here is read where it stands. */
    if (!is_digit((unsigned char) *cursor)) {
        int len = logical_line();

        if_len = 0;
        if_depth = 0;
        if_expand(body ? body : "", len);
        if_put("", 1);
        p = line_blanks(if_text);
        expanded = 1;
    } else {
        p = cursor;
    }

    while (is_digit((unsigned char) *p)) {
        n = n * 10 + (*p++ - '0');
        digits = 1;
        if (n > 0xffffff)
            acc_error_at(line, "the line number in a #line is too large");
    }
    if (!digits)
        acc_error_at(line, "#line needs a line number");

    p = line_blanks(p);
    if (*p == '"') {
        const char *start;
        char *keep;

        p++;
        start = p;
        while (*p && *p != '"' && *p != '\n')
            p++;
        if (*p != '"')
            acc_error_at(line, "the file name in a #line is not closed");

        keep = malloc((size_t) (p - start) + 1);
        if (!keep)
            acc_error("out of memory for a #line");
        memcpy(keep, start, (size_t) (p - start));
        keep[p - start] = '\0';
        p++;

        /* The name belongs to this level now, and whatever it had before
         * goes -- which for a file is the copy push_source made. */
        if (src_owned & OWN_PATH)
            free((char *) src_path);
        src_path = keep;
        src_owned |= OWN_PATH;
    }

    p = blanks_and_comments(p);
    if (*p && *p != '\n')
        acc_error_at(line, "#line takes a number and a file name, and "
                           "nothing else");

    if (!expanded) {
        cursor = (char *) p;
        rest_of_line();
    }
    line = (int) n - 1;
}

/* Every directive in a row, and whatever space and comments follow them, so
 * that next() comes back to a real token. An `#include` leaves the cursor at
 * the start of the file it names, which may itself begin with directives.
 *
 * Out of line: a compile that uses no directive at all pays one test. */
__attribute__((noinline))
void directives(void)
{
    for (;;) {
        int started = line;

        directive();
        skip_space();
        if (!*cursor)
            window_more();

        /* Another one only if it is first on its line too, which after a
         * directive means the line moved on. */
        if (!hash_at(cursor) || line == started)
            return;
    }
}

/* One directive, with the cursor on its '#'. The line it is on is consumed,
 * up to but not including its newline. */
static void directive(void)
{
    char name[32];

    cursor += hash_at(cursor);          /* the '#', or '%:' */
    skip_blanks();

    /* `# 200 "file"`, which is a #line with the word left out. Nothing
     * writes it by hand, but it is what a preprocessor puts in front of the
     * text it produces, and acc reads such a file whenever one is handed to
     * it. The digits are left where they are for do_line to read. */
    if (is_digit((unsigned char) *cursor)) {
        do_line();

        return;
    }
    if (!directive_name(name, (int) sizeof name)) {
        rest_of_line();                 /* a '#' alone, which C allows */

        return;
    }

    if (!strcmp(name, "include")) {
        do_include();

        return;
    }
    if (!strcmp(name, "define")) {
        do_define();

        return;
    }
    if (!strcmp(name, "undef")) {
        do_undef();

        return;
    }
    if (!strcmp(name, "if")) {
        do_if();

        return;
    }
    if (!strcmp(name, "elif")) {
        do_elif();

        return;
    }
    if (!strcmp(name, "ifdef")) {
        do_ifdef(1);

        return;
    }
    if (!strcmp(name, "ifndef")) {
        do_ifdef(0);

        return;
    }
    if (!strcmp(name, "else")) {
        do_else();

        return;
    }
    if (!strcmp(name, "endif")) {
        do_endif();

        return;
    }
    if (!strcmp(name, "error")) {
        do_error();

        return;
    }
    if (!strcmp(name, "pragma")) {
        do_pragma();

        return;
    }
    if (!strcmp(name, "line")) {
        do_line();

        return;
    }

    acc_error_at(line, "'#%s' is not a directive acc knows", name);
}
