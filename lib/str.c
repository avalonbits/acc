/*
 * What of <string.h> is not assembly: strtok, strcoll and strxfrm. The
 * rest is lib/strlen.s and the files beside it. And GNU C's
 * __builtin_ffs and __builtin_popcount, which the compiler declares in
 * every file.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <string.h>

/* The next token of `s`, or of where the last call stopped when `s` is
 * null, with the delimiter after it overwritten by a null. */
char *strtok(char *s, const char *delim)
{
    static char *next;
    char *end;

    if (!s)
        s = next;
    if (!s)
        return NULL;
    s += strspn(s, delim);
    if (!*s) {
        next = NULL;

        return NULL;
    }
    end = s + strcspn(s, delim);
    if (*end)
        *end++ = 0;
    else
        end = NULL;
    next = end;

    return s;
}

/* The C locale collates by the values of the bytes, as strcmp does, so a
 * transformed string is the string itself. */
int strcoll(const char *a, const char *b)
{
    return strcmp(a, b);
}

size_t strxfrm(char *to, const char *from, size_t n)
{
    size_t len = strlen(from);

    if (len < n)
        memcpy(to, from, len + 1);

    return len;
}

/* Where the lowest bit set in `value` is, counted from 1, or 0 for none:
 * a byte at a time, and then a bit at a time in the byte that has it. */
int __builtin_ffs(int value)
{
    unsigned bits = (unsigned) value;
    int at = 1;

    if (!bits)
        return 0;
    while (!(bits & 0xff)) {
        bits >>= 8;
        at += 8;
    }
    while (!(bits & 1)) {
        bits >>= 1;
        at++;
    }

    return at;
}

/* How many bits of `value` are set: each one cleared in turn, the lowest
 * first. */
int __builtin_popcount(unsigned value)
{
    int count = 0;

    while (value) {
        value &= value - 1;
        count++;
    }

    return count;
}
