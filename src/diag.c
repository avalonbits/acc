/*
 * Diagnostics: an error, where it is, in the words and the form gcc gives
 * one, and to the file -errors names.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "acc.h"
#include "ctype.h"
#include "fmt.h"
#include "timing.h"
#include "version.h"
#include "parse_int.h"

const char *lex_path(void);

/* ------------------------------------------------------------------ */
/* diagnostics                                                         */

/* -errors: where to write the error as well, for a program that runs acc
 * and needs to know what went wrong -- an editor that jumps to the line. It
 * can't read what acc prints: MOS has no output redirection, and one program
 * can't capture another's console. So the error goes to this file too, as
 *
 *     file:line:column: error: text
 *
 * with the column 0 when it is not known. An error with no line -- a link
 * that can't find a symbol, say -- is `acc:0:0: error: text`. A compile that
 * works removes the file, so a reader never finds a stale error.
 *
 * It also changes how acc fails: with 100, whatever went wrong. On the Agon
 * MOS reads a program's result as one of its own errors and rewrites 1, 4
 * and 5 into "Invalid command" -- a failed compile then looks like acc isn't
 * there at all. 100 is past MOS's table: MOS hands it back unchanged and
 * prints nothing for it. */
const char *errors_path;
int errors_asked;

__attribute__((noreturn)) static void fail(const char *file, int line, int col,
                                           const char *msg)
{
    out_abandon();
    obj_abandon();
    if (errors_path) {
        FILE *f = fopen(errors_path, "w");

        if (f) {
            fprintf(f, "%s:%d:%d: error: %s\n", file, line, col, msg);
            fclose(f);
        }
    }
    exit(errors_asked ? ERRORS_EXIT : ERROR_EXIT);
}

/* An error at a line and a column, which is printed as gcc prints one:
 * `file:line:column: error: text`, or without the column when it is 0,
 * not known. */
__attribute__((noreturn)) static void error_pos(int line, int col,
                                                const char *fmt, va_list ap)
{
    char msg[256];
    const char *file = lex_path() ? lex_path() : "acc";

    vsnprintf(msg, sizeof msg, fmt, ap);
    if (col)
        fprintf(stderr, "%s:%d:%d: error: %s\n", file, line, col, msg);
    else
        fprintf(stderr, "%s:%d: error: %s\n", file, line, msg);
    fail(file, line, col, msg);
}

void acc_error_pos(int line, int col, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_pos(line, col, fmt, ap);
}

void acc_error_spot(int line, const char *spot, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_pos(line, lex_col_at(spot), fmt, ap);
}

void acc_error_prev(const char *fmt, ...)
{
    int line = tok_line, col = lex_name_col(tok_at, &line);
    va_list ap;

    va_start(ap, fmt);
    error_pos(line, col ? col : lex_col(), fmt, ap);
}

/* At the name before the token the parser kept `at` of, on `line` -- the
 * name a call's `(` or an assignment's `=` follows -- or at that token when
 * what is before it is not a name. */
__attribute__((noreturn)) void error_before(int line, const char *at,
                                                   const char *fmt, ...)
{
    int col = lex_name_col(at, &line);
    va_list ap;

    va_start(ap, fmt);
    error_pos(line, col ? col : lex_col_at(at), fmt, ap);
}

/* At a line, and at the current token's column when it is the current
 * token's line: that is the token nearly every caller means. One that
 * means an earlier token kept where it was, and says so with
 * acc_error_spot. */
void acc_error_at(int line, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    error_pos(line, line == tok_line ? lex_col() : 0, fmt, ap);
}

void acc_error(const char *fmt, ...)
{
    char msg[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    fprintf(stderr, "acc: error: %s\n", msg);
    fail("acc", 0, 0, msg);
}
