/*
 * strdup and strndup: a string, or the start of one, copied into room from
 * the heap.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * In a file of its own, because it is the only one of <string.h> that wants
 * the heap. That was what kept the heap out of every program that measures
 * a string, while a link took a library member whole. A link takes the
 * functions a program reaches now, so where it lives no longer matters.
 */
#include <stdlib.h>
#include <string.h>

char *strdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *to = malloc(n);

    if (to)
        memcpy(to, s, n);

    return to;
}

/* At most n bytes of s, and a terminator: s need not have one inside
 * them. */
char *strndup(const char *s, size_t n)
{
    size_t len = 0;
    char *to;

    while (len < n && s[len])
        len++;
    to = malloc(len + 1);
    if (to) {
        memcpy(to, s, len);
        to[len] = 0;
    }

    return to;
}
