/*
 * Diagnostics: reporting an error and stopping. See diag.c.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef ACC_DIAG_H
#define ACC_DIAG_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Both report and do not return, and are declared so: a caller then has
 * nothing to keep for after the call, which in the lexer was a stack frame on
 * every token to hold values only an error path would have gone on to use. */
__attribute__((noreturn)) void acc_error(const char *fmt, ...);
__attribute__((noreturn)) void acc_error_at(int line, const char *fmt, ...);
__attribute__((noreturn)) void acc_error_pos(int line, int col,
                                             const char *fmt, ...);
/* At the token the parser kept tok_at of, as `spot`, on `line`. */
__attribute__((noreturn)) void acc_error_spot(int line, const char *spot,
                                              const char *fmt, ...);
/* At the name before the current token, that the parser has stepped past
 * to see what follows it -- or at the current token, when the one before
 * it is not a name. */
__attribute__((noreturn)) void acc_error_prev(const char *fmt, ...);

#endif
