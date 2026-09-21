/*
 * The memory half of <string.h>.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <string.h>

/* One file rather than one per function, so that a program that uses one of
 * these carries the five of them: a few hundred bytes, against a file each
 * and a link that has to look at all of them. Where that stops being the
 * right trade is when one of them grows. */

void *memcpy(void *to, const void *from, size_t n)
{
    char *d = to;
    const char *s = from;

    while (n--)
        *d++ = *s++;

    return to;
}

/* Backwards when the two overlap the other way round, which is the whole
 * difference between this and memcpy. */
void *memmove(void *to, const void *from, size_t n)
{
    char *d = to;
    const char *s = from;

    if (d <= s || d >= s + n)
        return memcpy(to, from, n);

    d += n;
    s += n;
    while (n--)
        *--d = *--s;

    return to;
}

void *memset(void *s, int c, size_t n)
{
    char *d = s;

    while (n--)
        *d++ = (char) c;

    return s;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = a, *q = b;

    while (n--) {
        if (*p != *q)
            return *p - *q;
        p++;
        q++;
    }

    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;

    while (n--) {
        if (*p == (unsigned char) c)
            return (void *) p;
        p++;
    }

    return NULL;
}
