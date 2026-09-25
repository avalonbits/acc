/*
 * <inttypes.h>'s functions, over intmax_t -- a long long here.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The four readers are strtol.c's reader with intmax_t's limits, and for
 * the wide two, a wchar_t's width.
 */
#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>

unsigned long long __acc_strtou(const char *s, char **end, int base, int step,
                                unsigned long long pos, unsigned long long neg,
                                int *negative);

intmax_t imaxabs(intmax_t j)
{
    return j < 0 ? -j : j;
}

imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom)
{
    imaxdiv_t r;

    r.quot = numer / denom;
    r.rem = numer % denom;

    return r;
}

intmax_t strtoimax(const char *s, char **end, int base)
{
    return strtoll(s, end, base);
}

uintmax_t strtoumax(const char *s, char **end, int base)
{
    return strtoull(s, end, base);
}

intmax_t wcstoimax(const wchar_t *s, wchar_t **end, int base)
{
    int negative;
    uintmax_t v = __acc_strtou((const char *) s, (char **) end, base,
                               sizeof (wchar_t), INTMAX_MAX,
                               (uintmax_t) INTMAX_MAX + 1, &negative);

    return (intmax_t) (negative ? -v : v);
}

uintmax_t wcstoumax(const wchar_t *s, wchar_t **end, int base)
{
    int negative;
    uintmax_t v = __acc_strtou((const char *) s, (char **) end, base,
                               sizeof (wchar_t), UINTMAX_MAX, UINTMAX_MAX,
                               &negative);

    return negative ? -v : v;
}
