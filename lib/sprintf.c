/*
 * sprintf, vsprintf and vprintf: the forms of printf most programs never
 * call, in a member of their own so that the ones that do not carry none of
 * them. Each is the formatter behind printf and snprintf, pointed somewhere
 * else.
 *
 * sprintf is snprintf told the buffer has no end. C99 asks the caller to
 * have made it big enough, and there is no size to check against -- which
 * is why snprintf is the one to use, and why sprintf is still C99.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stdio.h>

int acc_format(const char *fmt, va_list ap);

extern FILE *acc_sink_file;
extern char *acc_sink_buf;

int vsprintf(char *to, const char *fmt, va_list ap)
{
    return vsnprintf(to, (size_t) -1, fmt, ap);
}

int sprintf(char *to, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(to, (size_t) -1, fmt, ap);
    va_end(ap);

    return n;
}

int vprintf(const char *fmt, va_list ap)
{
    acc_sink_file = NULL;
    acc_sink_buf = NULL;

    return acc_format(fmt, ap);
}
