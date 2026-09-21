/*
 * The string half of <string.h>.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <string.h>

size_t strlen(const char *s)
{
    const char *p = s;

    while (*p)
        p++;

    return (size_t) (p - s);
}

char *strcpy(char *to, const char *from)
{
    char *d = to;

    while ((*d++ = *from++) != 0)
        ;

    return to;
}

/* C's own peculiarity, and it is not a mistake here: what is copied is
 * padded out with zeros to n, and a string exactly n long is left without
 * one. */
char *strncpy(char *to, const char *from, size_t n)
{
    char *d = to;

    while (n && *from) {
        *d++ = *from++;
        n--;
    }
    while (n--)
        *d++ = 0;

    return to;
}

char *strcat(char *to, const char *from)
{
    char *d = to;

    while (*d)
        d++;
    while ((*d++ = *from++) != 0)
        ;

    return to;
}

char *strncat(char *to, const char *from, size_t n)
{
    char *d = to;

    while (*d)
        d++;
    while (n && *from) {
        *d++ = *from++;
        n--;
    }
    *d = 0;

    return to;
}

/* Compared as unsigned char, which is what C says and is not what comparing
 * the chars would give on this machine: its char is signed, so a byte past
 * 127 would come out less than a space. */
int strcmp(const char *a, const char *b)
{
    const unsigned char *p = (const unsigned char *) a;
    const unsigned char *q = (const unsigned char *) b;

    while (*p && *p == *q) {
        p++;
        q++;
    }

    return *p - *q;
}

int strncmp(const char *a, const char *b, size_t n)
{
    const unsigned char *p = (const unsigned char *) a;
    const unsigned char *q = (const unsigned char *) b;

    while (n && *p && *p == *q) {
        p++;
        q++;
        n--;
    }

    return n ? *p - *q : 0;
}

/* The terminating zero counts as part of the string, so strchr(s, 0) finds
 * the end of it. */
char *strchr(const char *s, int c)
{
    for (; *s; s++)
        if (*s == (char) c)
            return (char *) s;

    return (char) c ? NULL : (char *) s;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;

    for (;; s++) {
        if (*s == (char) c)
            last = s;
        if (!*s)
            break;
    }

    return (char *) last;
}

char *strstr(const char *hay, const char *needle)
{
    size_t n = strlen(needle);

    if (!n)
        return (char *) hay;
    for (; *hay; hay++)
        if (*hay == *needle && strncmp(hay, needle, n) == 0)
            return (char *) hay;

    return NULL;
}
