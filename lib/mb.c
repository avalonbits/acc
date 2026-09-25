/*
 * Multibyte characters and wide ones, one into the other: <wchar.h>'s
 * restartable conversions, and <stdlib.h>'s older ones over them.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The multibyte characters are UTF-8, as far as a sixteen-bit wchar_t
 * goes: one byte below 0x80, two below 0x800 and three for the rest of the
 * first plane. What is not a character is refused with EILSEQ, as glibc
 * refuses it: a byte that cannot start one, one that cannot continue one,
 * a character written with more bytes than it needs, and the surrogates,
 * which are halves of characters and not characters. A four-byte sequence
 * is a real character, but past what a wchar_t here can hold, so it is
 * refused too -- the one place this parts from a UTF-8 locale elsewhere.
 *
 * mbstate_t carries a character that stopped in the middle, so that the
 * rest can come in the next call.
 */
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define BAD     ((size_t) -1)
#define PARTIAL ((size_t) -2)

static mbstate_t mbrtowc_state, mbrlen_state, wcrtomb_state;
static mbstate_t mbsrtowcs_state, wcsrtombs_state;

int mbsinit(const mbstate_t *ps)
{
    return !ps || !ps->need;
}

wint_t btowc(int c)
{
    return c >= 0 && c < 0x80 ? (wint_t) c : WEOF;
}

int wctob(wint_t c)
{
    return c >= 0 && c < 0x80 ? c : EOF;
}

static size_t bad(mbstate_t *ps)
{
    ps->need = 0;
    errno = EILSEQ;

    return BAD;
}

size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps)
{
    size_t used = 0;
    unsigned c;

    if (!ps)
        ps = &mbrtowc_state;
    if (!s) {
        s = "";
        n = 1;
        pwc = NULL;
    }
    if (!n)
        return PARTIAL;

    if (!ps->need) {
        c = (unsigned char) *s++;
        used++;
        if (c < 0x80) {
            if (pwc)
                *pwc = (wchar_t) c;

            return c ? 1 : 0;
        }
        if (c < 0xc2 || c > 0xef)
            return bad(ps);             /* a continuation, an overlong, or
                                         * four bytes and more */
        if (c < 0xe0) {
            ps->need = 1;
            ps->value = (unsigned short) (c & 0x1f);
            ps->min = 0;
        } else {
            ps->need = 2;
            ps->value = (unsigned short) (c & 0x0f);
            ps->min = 1;                /* checked below */
        }
    }
    while (ps->need) {
        if (used == n)
            return PARTIAL;
        c = (unsigned char) *s++;
        used++;
        if ((c & 0xc0) != 0x80)
            return bad(ps);
        ps->value = (unsigned short) (ps->value << 6 | (c & 0x3f));
        ps->need--;
    }

    /* A three-byte character is at least 0x800 and is not one of the
     * surrogates. Found at the last byte, as glibc finds it, though the
     * second already shows it. */
    if (ps->min) {
        ps->min = 0;
        if (ps->value < 0x800 || (ps->value >= 0xd800 && ps->value < 0xe000))
            return bad(ps);
    }
    if (pwc)
        *pwc = (wchar_t) ps->value;

    return ps->value ? used : 0;
}

size_t mbrlen(const char *s, size_t n, mbstate_t *ps)
{
    return mbrtowc(NULL, s, n, ps ? ps : &mbrlen_state);
}

size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps)
{
    unsigned c = (unsigned short) wc;
    char buf[3];

    if (!ps)
        ps = &wcrtomb_state;
    if (!s) {
        s = buf;
        c = 0;
    }
    ps->need = 0;
    if (c < 0x80) {
        s[0] = (char) c;

        return 1;
    }
    if (c < 0x800) {
        s[0] = (char) (0xc0 | c >> 6);
        s[1] = (char) (0x80 | (c & 0x3f));

        return 2;
    }
    if (c >= 0xd800 && c < 0xe000)
        return bad(ps);
    s[0] = (char) (0xe0 | c >> 12);
    s[1] = (char) (0x80 | ((c >> 6) & 0x3f));
    s[2] = (char) (0x80 | (c & 0x3f));

    return 3;
}

/* A whole string of characters, into at most `len` wide ones. *src is
 * left past the last one converted, or null if the terminator was. */
size_t mbsrtowcs(wchar_t *dst, const char **src, size_t len, mbstate_t *ps)
{
    const char *s = *src;
    size_t n = 0, r;
    wchar_t wc;

    if (!ps)
        ps = &mbsrtowcs_state;
    while (!dst || n < len) {
        r = mbrtowc(&wc, s, 3, ps);
        if (r == BAD) {
            if (dst)
                *src = s;

            return BAD;
        }
        if (r == 0) {
            if (dst) {
                dst[n] = 0;
                *src = NULL;
            }

            return n;
        }
        if (dst)
            dst[n] = wc;
        s += r;
        n++;
    }
    *src = s;

    return n;
}

/* And the other way: the bytes of each wide character, into at most `len`
 * bytes, and never part of one. */
size_t wcsrtombs(char *dst, const wchar_t **src, size_t len, mbstate_t *ps)
{
    const wchar_t *s = *src;
    size_t n = 0, r;
    char b[3];

    if (!ps)
        ps = &wcsrtombs_state;
    for (;; s++) {
        r = wcrtomb(b, *s, ps);
        if (r == BAD) {
            if (dst)
                *src = s;

            return BAD;
        }
        if (dst && n + r > len) {
            *src = s;

            return n;
        }
        if (!*s) {
            if (dst) {
                dst[n] = 0;
                *src = NULL;
            }

            return n;
        }
        if (dst) {
            size_t i;

            for (i = 0; i < r; i++)
                dst[n + i] = b[i];
        }
        n += r;
    }
}

/* <stdlib.h>'s, which have no state of their own to speak of: UTF-8 has
 * no shift states, so asking with a null pointer answers 0. */
int mblen(const char *s, size_t n)
{
    mbstate_t st = { 0, 0, 0 };
    size_t r;

    if (!s)
        return 0;
    r = mbrtowc(NULL, s, n, &st);

    return r == BAD || r == PARTIAL ? -1 : (int) r;
}

int mbtowc(wchar_t *pwc, const char *s, size_t n)
{
    mbstate_t st = { 0, 0, 0 };
    size_t r;

    if (!s)
        return 0;
    r = mbrtowc(pwc, s, n, &st);
    if (r == PARTIAL) {
        errno = EILSEQ;

        return -1;
    }

    return r == BAD ? -1 : (int) r;
}

int wctomb(char *s, wchar_t wc)
{
    mbstate_t st = { 0, 0, 0 };
    size_t r;

    if (!s)
        return 0;
    r = wcrtomb(s, wc, &st);

    return r == BAD ? -1 : (int) r;
}

size_t mbstowcs(wchar_t *dst, const char *src, size_t n)
{
    mbstate_t st = { 0, 0, 0 };

    return mbsrtowcs(dst, &src, n, &st);
}

size_t wcstombs(char *dst, const wchar_t *src, size_t n)
{
    mbstate_t st = { 0, 0, 0 };

    return wcsrtombs(dst, &src, n, &st);
}
