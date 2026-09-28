/*
 * acc -- a C compiler for the Agon Light.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * printf, for the formats acc uses and no others.
 *
 * agondev's printf is nanoprintf: floats, precisions, every length and flag,
 * and five kilobytes of acc.bin with the helpers it brings -- five
 * kilobytes of heap, on a machine where the heap is what decides which
 * programs compile. acc prints messages, a map and a list of offsets, and
 * what they ask for is %s, with a precision as %.*s, %c, %%, and %d, %u and
 * %x with an optional `l`, a width and a `0` to pad with. That is all this
 * does; anything else comes out as written.
 *
 * In agondev's build it is printf, fprintf, vfprintf, snprintf and
 * vsnprintf, so that libagon's are not linked. Elsewhere it is only
 * fmt_vsnprintf, which test/test_fmt.c holds to the C library's answers.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "fmt.h"

/* agondev's build packs the words of acc's error messages into single bytes
 * (src/msgpack.py), and a format is where they are expanded again. */
#ifdef ACC_MSG_PACKED
#include "msgdict.h"
#endif

/* Where the characters go: a buffer of `cap`, of which `len` would have
 * been written, or a file, through `chunk` a piece at a time. */
typedef struct {
    char   *buf;
    size_t  cap, len;
    FILE   *file;
    char    chunk[64];
    size_t  held;
} Sink;

static void sink_flush(Sink *s)
{
    if (s->held)
        fwrite(s->chunk, 1, s->held, s->file);
    s->held = 0;
}

static void sink_put(Sink *s, char c)
{
    if (s->file) {
        if (s->held == sizeof s->chunk)
            sink_flush(s);
        s->chunk[s->held++] = c;
    } else if (s->len + 1 < s->cap) {
        s->buf[s->len] = c;
    }
    s->len++;
}

#ifdef ACC_MSG_PACKED
/* The word a packed byte stands for: the one after that many NULs. */
static void put_word(Sink *s, unsigned char code)
{
    const char *w = msg_words;

    for (; code != MSG_FIRST; code--)
        w += strlen(w) + 1;
    while (*w)
        sink_put(s, *w++);
}
#endif

/* A number's digits, most significant first, padded to `width` with
 * `pad`; `neg` puts a minus sign in front of them. */
static void put_number(Sink *s, unsigned long v, unsigned base, int neg,
                       int width, char pad)
{
    char digits[12];
    int n = 0;

    do {
        unsigned d = (unsigned) (v % base);

        digits[n++] = (char) (d < 10 ? '0' + d : 'a' + d - 10);
        v /= base;
    } while (v);
    if (neg) {
        if (pad == '0')
            sink_put(s, '-');
        width--;
    }
    for (; width > n; width--)
        sink_put(s, pad);
    if (neg && pad != '0')
        sink_put(s, '-');
    while (n)
        sink_put(s, digits[--n]);
}

/* A long argument, read in a function of its own. Read where it is used,
 * in format's switch, agondev's clang advanced the list past the long and
 * then read the long's top byte from the advanced pointer -- nine bytes on
 * rather than three -- so every %lu came out with a stray top byte. Alone,
 * the same va_arg compiles as it should. */
typedef struct {
    va_list ap;                 /* in a struct, so it can be passed by address */
} Args;

__attribute__((noinline))
static unsigned long arg_long(Args *a)
{
    return va_arg(a->ap, unsigned long);
}

static void format(Sink *s, const char *fmt, Args *a)
{
    for (; *fmt; fmt++) {
        const char *start = fmt;
        char pad = ' ';
        int width = 0, lng = 0;
        unsigned most = (unsigned) -1;          /* %.*s's precision */

        if (*fmt != '%') {
#ifdef ACC_MSG_PACKED
            if ((unsigned char) *fmt >= MSG_FIRST) {
                put_word(s, (unsigned char) *fmt);
                continue;
            }
#endif
            sink_put(s, *fmt);
            continue;
        }
        fmt++;
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');
        if (fmt[0] == '.' && fmt[1] == '*') {   /* %.*s, the only one used */
            most = (unsigned) va_arg(a->ap, int);
            fmt += 2;
        }
        if (*fmt == 'l') {
            lng = 1;
            fmt++;
        }

        switch (*fmt) {
        case 'd': {
            long v = lng ? (long) arg_long(a) : va_arg(a->ap, int);

            put_number(s, v < 0 ? 0UL - (unsigned long) v : (unsigned long) v,
                       10, v < 0, width, pad);
            break;
        }
        case 'u':
        case 'x':
            put_number(s, lng ? arg_long(a) : va_arg(a->ap, unsigned),
                       *fmt == 'x' ? 16 : 10, 0, width, pad);
            break;
        case 'c':
            sink_put(s, (char) va_arg(a->ap, int));
            break;
        case 's': {
            const char *str = va_arg(a->ap, const char *);
            int n;

            if (!str)
                str = "(null)";
            for (n = 0; str[n] && (unsigned) n != most; n++)
                ;
            for (; width > n; width--)
                sink_put(s, ' ');
            while (n--)
                sink_put(s, *str++);
            break;
        }
        case '%':
            sink_put(s, '%');
            break;
        default:                        /* not one of acc's: as written */
            for (; start <= fmt && *start; start++)
                sink_put(s, *start);
            if (!*fmt)
                return;
            break;
        }
    }
}

int fmt_vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap)
{
    Sink s;
    Args args;

    s.buf = buf;
    s.cap = cap;
    s.len = 0;
    s.file = NULL;
    s.held = 0;
    va_copy(args.ap, ap);
    format(&s, fmt, &args);
    va_end(args.ap);
    if (cap)
        buf[s.len < cap ? s.len : cap - 1] = '\0';

    return (int) s.len;
}

#if defined(AGONDEV) && defined(__clang__)
FILE *fmt_console;
#endif

int fmt_vfprintf(FILE *file, const char *fmt, va_list ap)
{
    Sink s;
    Args args;

#if defined(AGONDEV) && defined(__clang__)
    if (fmt_console && (file == stdout || file == stderr))
        file = fmt_console;
#endif
    s.buf = NULL;
    s.cap = 0;
    s.len = 0;
    s.file = file;
    s.held = 0;
    va_copy(args.ap, ap);
    format(&s, fmt, &args);
    va_end(args.ap);
    sink_flush(&s);

    return (int) s.len;
}

/* agondev's build, in place of libagon's; and a host build that asks, so
 * that test/msgpack.sh can print the packed messages through this. */
#if (defined(AGONDEV) && defined(__clang__)) || defined(ACC_FMT_WRAP)
int vsnprintf(char *buf, size_t cap, const char *fmt, va_list ap)
{
    return fmt_vsnprintf(buf, cap, fmt, ap);
}

int snprintf(char *buf, size_t cap, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = fmt_vsnprintf(buf, cap, fmt, ap);
    va_end(ap);

    return n;
}

int vfprintf(FILE *file, const char *fmt, va_list ap)
{
    return fmt_vfprintf(file, fmt, ap);
}

int fprintf(FILE *file, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = fmt_vfprintf(file, fmt, ap);
    va_end(ap);

    return n;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = fmt_vfprintf(stdout, fmt, ap);
    va_end(ap);

    return n;
}
#endif
