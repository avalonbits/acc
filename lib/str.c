/*
 * The string half of <string.h>.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <ctype.h>
#include <string.h>

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

/* strncmp with the case of letters taken out, as tolower has it: what
 * the two compare as, in the C locale, is their lowercase. */
int strncasecmp(const char *a, const char *b, size_t n)
{
    const unsigned char *p = (const unsigned char *) a;
    const unsigned char *q = (const unsigned char *) b;

    while (n && *p && tolower(*p) == tolower(*q)) {
        p++;
        q++;
        n--;
    }

    return n ? tolower(*p) - tolower(*q) : 0;
}

/* As strchr, but a character not there finds the end of the string rather
 * than nothing. */
char *strchrnul(const char *s, int c)
{
    while (*s && *s != (char) c)
        s++;

    return (char *) s;
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

/* How many of s's first characters are all in `accept`, and how many are
 * all not in `reject`. */
size_t strspn(const char *s, const char *accept)
{
    size_t n = 0;

    while (s[n] && strchr(accept, s[n]))
        n++;

    return n;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t n = 0;

    while (s[n] && !strchr(reject, s[n]))
        n++;

    return n;
}

char *strpbrk(const char *s, const char *accept)
{
    s += strcspn(s, accept);

    return *s ? (char *) s : NULL;
}

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
