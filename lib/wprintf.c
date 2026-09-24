/*
 * The wide printf family: wprintf, fwprintf, swprintf and the v forms.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The format is walked here, and what only a wide printf does is done here:
 * %s and %c, whose width and precision count wide characters, and which
 * convert a narrow string or character to wide ones. Every other conversion
 * -- the numbers, floating and whole, and the pointer -- writes nothing but
 * ASCII, so it is handed to printf's own formatter, one conversion at a
 * time, and its characters come back here to be written wide. The two can
 * then never disagree about how a number is written.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <wchar.h>

extern FILE  *acc_sink_file;
extern char  *acc_sink_buf;
extern int  (*acc_sink_fn)(int);
int acc_format(const char *fmt, va_list ap);

/* Where the wide characters go: a buffer with room for `cap`, or a file. */
typedef struct {
    wchar_t *buf;
    size_t   cap, at;
    FILE    *file;
    int      count;
    int      bad;
} Out;

static Out *out;

static void put(wchar_t c)
{
    if (out->buf) {
        if (out->at + 1 < out->cap)
            out->buf[out->at] = c;
        else
            out->bad = 1;       /* swprintf answers negative for this */
        out->at++;
    } else if (fputwc(c, out->file) == WEOF) {
        out->bad = 1;
    }
    out->count++;
}

static int byte(int c)
{
    put((wchar_t) (unsigned char) c);

    return c;
}

/* One conversion, through printf's formatter. */
static void narrow(const char *spec, ...)
{
    va_list ap;

    va_start(ap, spec);
    acc_sink_buf = NULL;
    acc_sink_file = NULL;
    acc_sink_fn = byte;
    acc_format(spec, ap);
    acc_sink_fn = NULL;
    va_end(ap);
}

static void pad(int n)
{
    while (n-- > 0)
        put(L' ');
}

/* A number into `p`, for rebuilding a width or a precision given as *. */
static char *decimal(char *p, int v)
{
    char digits[8];
    int n = 0;

    do
        digits[n++] = (char) ('0' + v % 10);
    while (v /= 10);
    while (n)
        *p++ = digits[--n];

    return p;
}

/* The characters of a narrow string as wide ones: up to `max` of them, or
 * all when max is negative, into `put` when `emit` is set. Answers how
 * many there are. */
static int narrow_string(const char *s, int max, int emit)
{
    mbstate_t st = { 0, 0, 0 };
    wchar_t wc;
    size_t r;
    int n = 0;

    while (*s && (max < 0 || n < max)) {
        r = mbrtowc(&wc, s, 3, &st);
        if (r == (size_t) -1 || r == (size_t) -2)
            break;
        if (emit)
            put(wc);
        s += r;
        n++;
    }

    return n;
}

static int wformat(const wchar_t *fmt, va_list ap)
{
    while (*fmt) {
        char spec[40], *sp = spec;
        int left = 0, width = 0, prec = -1, longs = 0, shorts = 0, c;

        if (*fmt != L'%') {
            put(*fmt++);
            continue;
        }
        fmt++;
        *sp++ = '%';
        for (; *fmt == L'-' || *fmt == L'+' || *fmt == L' ' || *fmt == L'#'
               || *fmt == L'0'; fmt++) {
            left |= *fmt == L'-';
            *sp++ = (char) *fmt;
        }
        if (*fmt == L'*') {
            fmt++;
            width = va_arg(ap, int);
            if (width < 0) {
                left = 1;
                *sp++ = '-';
                width = -width;
            }
        } else {
            while (*fmt >= L'0' && *fmt <= L'9')
                width = width * 10 + (*fmt++ - L'0');
        }
        if (width)
            sp = decimal(sp, width);
        if (*fmt == L'.') {
            fmt++;
            prec = 0;
            if (*fmt == L'*') {
                fmt++;
                prec = va_arg(ap, int);
            } else {
                while (*fmt >= L'0' && *fmt <= L'9')
                    prec = prec * 10 + (*fmt++ - L'0');
            }
            if (prec >= 0) {
                *sp++ = '.';
                sp = decimal(sp, prec);
            }
        }
        for (;; fmt++) {
            if (*fmt == L'l')      longs++;
            else if (*fmt == L'h') shorts++;
            else if (*fmt == L'j') longs = 2;
            else if (*fmt != L'z' && *fmt != L't' && *fmt != L'L') break;
        }
        c = *fmt;
        if (!c)
            break;
        fmt++;

        switch (c) {
        case 'd': case 'i': case 'o': case 'u': case 'x': case 'X':
            if (shorts)
                *sp++ = 'h';
            if (shorts > 1)
                *sp++ = 'h';
            *sp++ = 'l';
            *sp++ = 'l';
            *sp++ = (char) c;
            *sp = 0;
            if (c == 'd' || c == 'i')
                narrow(spec, longs > 1 ? va_arg(ap, long long)
                           : longs ? (long long) va_arg(ap, long)
                                   : (long long) va_arg(ap, int));
            else
                narrow(spec, longs > 1 ? va_arg(ap, unsigned long long)
                           : longs ? (unsigned long long) va_arg(ap, unsigned long)
                                   : (unsigned long long) va_arg(ap, unsigned));
            break;

        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
        case 'a': case 'A':
            *sp++ = (char) c;
            *sp = 0;
            narrow(spec, va_arg(ap, double));
            break;

        case 'p':
            *sp++ = 'p';
            *sp = 0;
            narrow(spec, va_arg(ap, void *));
            break;

        case 'c': {
            /* A narrow character is converted as btowc would. */
            int wc = va_arg(ap, int);

            if (!longs)
                wc = (int) btowc(wc);
            if (!left)
                pad(width - 1);
            put((wchar_t) wc);
            if (left)
                pad(width - 1);
            break;
        }

        case 's': {
            int n;

            if (longs) {
                const wchar_t *s = va_arg(ap, const wchar_t *);

                if (!s)
                    s = L"(null)";
                for (n = 0; s[n] && (prec < 0 || n < prec); n++)
                    ;
                if (!left)
                    pad(width - n);
                for (c = 0; c < n; c++)
                    put(s[c]);
            } else {
                const char *s = va_arg(ap, const char *);

                if (!s)
                    s = "(null)";
                n = narrow_string(s, prec, 0);
                if (!left)
                    pad(width - n);
                narrow_string(s, prec, 1);
            }
            if (left)
                pad(width - n);
            break;
        }

        case 'n': {
            void *to = va_arg(ap, void *);

            if (longs > 1)
                *(long long *) to = out->count;
            else if (longs)
                *(long *) to = out->count;
            else if (shorts > 1)
                *(signed char *) to = (signed char) out->count;
            else if (shorts)
                *(short *) to = (short) out->count;
            else
                *(int *) to = out->count;
            break;
        }

        case '%':
            put(L'%');
            break;

        default:
            put(L'%');
            put((wchar_t) c);
        }
    }

    return out->count;
}

int vswprintf(wchar_t *s, size_t n, const wchar_t *fmt, va_list ap)
{
    Out o = { 0, 0, 0, 0, 0, 0 };
    int r;

    o.buf = s;
    o.cap = n;
    out = &o;
    r = wformat(fmt, ap);
    if (n)
        s[o.at < n ? o.at : n - 1] = 0;

    return o.bad ? -1 : r;
}

int swprintf(wchar_t *s, size_t n, const wchar_t *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vswprintf(s, n, fmt, ap);
    va_end(ap);

    return r;
}

int vfwprintf(FILE *f, const wchar_t *fmt, va_list ap)
{
    Out o = { 0, 0, 0, 0, 0, 0 };
    int r;

    o.file = f;
    out = &o;
    r = wformat(fmt, ap);

    return o.bad ? -1 : r;
}

int fwprintf(FILE *f, const wchar_t *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vfwprintf(f, fmt, ap);
    va_end(ap);

    return r;
}

int vwprintf(const wchar_t *fmt, va_list ap)
{
    return vfwprintf(stdout, fmt, ap);
}

int wprintf(const wchar_t *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vfwprintf(stdout, fmt, ap);
    va_end(ap);

    return r;
}
