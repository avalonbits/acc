/*
 * The numbers of <wchar.h>: strtol's and strtod's readers, stepping two
 * bytes at a time.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <limits.h>
#include <stddef.h>
#include <wchar.h>

unsigned long long __acc_strtou(const char *s, char **end, int base, int step,
                                unsigned long long pos, unsigned long long neg,
                                int *negative);
double __acc_strtod(const char *s, char **end, int step);

#define W sizeof (wchar_t)

long wcstol(const wchar_t *s, wchar_t **end, int base)
{
    int negative;
    unsigned long v = (unsigned long) __acc_strtou((const char *) s, (char **) end,
                                                   base, W, LONG_MAX,
                                                   (unsigned long) LONG_MAX + 1,
                                                   &negative);

    return (long) (negative ? -v : v);
}

unsigned long wcstoul(const wchar_t *s, wchar_t **end, int base)
{
    int negative;
    unsigned long v = (unsigned long) __acc_strtou((const char *) s, (char **) end,
                                                   base, W, ULONG_MAX, ULONG_MAX,
                                                   &negative);

    return negative ? -v : v;
}

long long wcstoll(const wchar_t *s, wchar_t **end, int base)
{
    int negative;
    unsigned long long v = __acc_strtou((const char *) s, (char **) end, base, W,
                                        LLONG_MAX, (unsigned long long) LLONG_MAX + 1,
                                        &negative);

    return (long long) (negative ? -v : v);
}

unsigned long long wcstoull(const wchar_t *s, wchar_t **end, int base)
{
    int negative;
    unsigned long long v = __acc_strtou((const char *) s, (char **) end, base, W,
                                        ULLONG_MAX, ULLONG_MAX, &negative);

    return negative ? -v : v;
}

double wcstod(const wchar_t *s, wchar_t **end)
{
    return __acc_strtod((const char *) s, (char **) end, W);
}

float wcstof(const wchar_t *s, wchar_t **end)
{
    return (float) __acc_strtod((const char *) s, (char **) end, W);
}
