/*
 * The wide scanf family: scanf.c's reader, over a wide format and wide
 * characters from a wide string or a stream.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

typedef struct {
    int  (*get)(void *src);
    void (*unget)(int c, void *src);
    void  *src;
    int    count;
} Input;

int __acc_scan(Input *in, const char *fmt, int fs, va_list ap);

static int string_get(void *src)
{
    const wchar_t **p = src;

    if (!**p)
        return EOF;

    return (unsigned short) *(*p)++;
}

static void string_unget(int c, void *src)
{
    const wchar_t **p = src;

    (void) c;
    --*p;
}

int vswscanf(const wchar_t *s, const wchar_t *fmt, va_list ap)
{
    Input in;

    in.get = string_get;
    in.unget = string_unget;
    in.src = &s;
    in.count = 0;

    return __acc_scan(&in, (const char *) fmt, sizeof (wchar_t), ap);
}

int swscanf(const wchar_t *s, const wchar_t *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vswscanf(s, fmt, ap);
    va_end(ap);

    return n;
}

static int stream_get(void *src)
{
    wint_t c = fgetwc(src);

    return c == WEOF ? EOF : (int) c;
}

static void stream_unget(int c, void *src)
{
    ungetwc((wint_t) c, src);
}

int vfwscanf(FILE *f, const wchar_t *fmt, va_list ap)
{
    Input in;

    in.get = stream_get;
    in.unget = stream_unget;
    in.src = f;
    in.count = 0;

    return __acc_scan(&in, (const char *) fmt, sizeof (wchar_t), ap);
}

int fwscanf(FILE *f, const wchar_t *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vfwscanf(f, fmt, ap);
    va_end(ap);

    return n;
}

int vwscanf(const wchar_t *fmt, va_list ap)
{
    return vfwscanf(stdin, fmt, ap);
}

int wscanf(const wchar_t *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vfwscanf(stdin, fmt, ap);
    va_end(ap);

    return n;
}
