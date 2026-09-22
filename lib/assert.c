/*
 * What an assertion that failed says before it stops the program.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Its own member, so that a program compiled with NDEBUG -- where the macro
 * is nothing at all -- carries neither this nor the printf it calls.
 *
 * C99 7.2.1.1 says the message names the failing expression, the file, the
 * line and the function, and does not say in what words. These are the
 * words everything else uses, so that what is read on the screen is what
 * would be read anywhere else.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void __acc_assert_failed(const char *expr, const char *file, int line,
                         const char *fn)
{
    fprintf(stderr, "%s:%d: %s: Assertion `%s' failed.\n", file, line, fn,
            expr);
    abort();
}
