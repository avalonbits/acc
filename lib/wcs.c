/*
 * The wide strings of <wchar.h>, which are <string.h>'s over wchar_t.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * A wchar_t is signed here, and C compares wide strings by the values of
 * their wchar_t, so a character past 0x7fff sorts below 'a'. That is what
 * wcscmp answers and what wmemcmp answers; the C locale collates the same
 * way, so wcscoll is wcscmp and wcsxfrm a copy.
 */
#include <stddef.h>
#include <wchar.h>

size_t wcslen(const wchar_t *s)
{
    const wchar_t *p = s;

    while (*p)
        p++;

    return (size_t) (p - s);
}

wchar_t *wcscpy(wchar_t *to, const wchar_t *from)
{
    wchar_t *p = to;

    while ((*p++ = *from++) != 0)
        ;

    return to;
}

/* At most n, and padded with nulls to n when `from` is shorter. */
wchar_t *wcsncpy(wchar_t *to, const wchar_t *from, size_t n)
{
    size_t i;

    for (i = 0; i < n && from[i]; i++)
        to[i] = from[i];
    for (; i < n; i++)
        to[i] = 0;

    return to;
}

wchar_t *wcscat(wchar_t *to, const wchar_t *from)
{
    wcscpy(to + wcslen(to), from);

    return to;
}

wchar_t *wcsncat(wchar_t *to, const wchar_t *from, size_t n)
{
    wchar_t *p = to + wcslen(to);

    while (n-- && *from)
        *p++ = *from++;
    *p = 0;

    return to;
}

int wcscmp(const wchar_t *a, const wchar_t *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return *a < *b ? -1 : *a > *b;
}

int wcsncmp(const wchar_t *a, const wchar_t *b, size_t n)
{
    for (; n; n--, a++, b++) {
        if (*a != *b)
            return *a < *b ? -1 : 1;
        if (!*a)
            break;
    }

    return 0;
}

int wcscoll(const wchar_t *a, const wchar_t *b)
{
    return wcscmp(a, b);
}

size_t wcsxfrm(wchar_t *to, const wchar_t *from, size_t n)
{
    size_t len = wcslen(from);

    if (len < n)
        wcscpy(to, from);

    return len;
}

wchar_t *wcschr(const wchar_t *s, wchar_t c)
{
    for (;; s++) {
        if (*s == c)
            return (wchar_t *) s;
        if (!*s)
            return NULL;
    }
}

wchar_t *wcsrchr(const wchar_t *s, wchar_t c)
{
    const wchar_t *found = NULL;

    for (;; s++) {
        if (*s == c)
            found = s;
        if (!*s)
            return (wchar_t *) found;
    }
}

size_t wcsspn(const wchar_t *s, const wchar_t *accept)
{
    size_t n = 0;

    while (s[n] && wcschr(accept, s[n]))
        n++;

    return n;
}

size_t wcscspn(const wchar_t *s, const wchar_t *reject)
{
    size_t n = 0;

    while (s[n] && !wcschr(reject, s[n]))
        n++;

    return n;
}

wchar_t *wcspbrk(const wchar_t *s, const wchar_t *accept)
{
    s += wcscspn(s, accept);

    return *s ? (wchar_t *) s : NULL;
}

wchar_t *wcsstr(const wchar_t *hay, const wchar_t *needle)
{
    size_t n = wcslen(needle);

    for (; *hay; hay++)
        if (wcsncmp(hay, needle, n) == 0)
            return (wchar_t *) hay;

    return n ? NULL : (wchar_t *) hay;
}

/* The next token of `s`, or of where the last call left off when `s` is
 * null -- which is kept in *save, so that two strings can be walked at
 * once, unlike strtok. */
wchar_t *wcstok(wchar_t *s, const wchar_t *delim, wchar_t **save)
{
    wchar_t *end;

    if (!s)
        s = *save;
    if (!s)
        return NULL;
    s += wcsspn(s, delim);
    if (!*s) {
        *save = NULL;

        return NULL;
    }
    end = s + wcscspn(s, delim);
    if (*end)
        *end++ = 0;
    else
        end = NULL;
    *save = end;

    return s;
}

wchar_t *wmemcpy(wchar_t *to, const wchar_t *from, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
        to[i] = from[i];

    return to;
}

wchar_t *wmemmove(wchar_t *to, const wchar_t *from, size_t n)
{
    size_t i;

    if (to < from) {
        for (i = 0; i < n; i++)
            to[i] = from[i];
    } else {
        for (i = n; i > 0; i--)
            to[i - 1] = from[i - 1];
    }

    return to;
}

int wmemcmp(const wchar_t *a, const wchar_t *b, size_t n)
{
    for (; n; n--, a++, b++)
        if (*a != *b)
            return *a < *b ? -1 : 1;

    return 0;
}

wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n)
{
    for (; n; n--, s++)
        if (*s == c)
            return (wchar_t *) s;

    return NULL;
}

wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
        s[i] = c;

    return s;
}
