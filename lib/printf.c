/*
 * printf and its family -- snprintf, sprintf and the v forms of each --
 * which are one formatter pointed at the screen, a buffer or a file. A
 * link takes only the ones a program calls.
 *
 * What is here is the conversions a program on this machine actually writes:
 * signed and unsigned decimal, octal and hex, a character, a string, a
 * pointer, and `%%`. With the flags `-`, `0`, `+`, `#` and a space, a width
 * and a precision that can each be a `*`, the lengths `h`, `hh`, `l`, `ll`,
 * `j`, `z` and `t`, and `%n`. The floating conversions are in
 * printf_float.c, reached through a weak reference that only a program
 * passing a float asks for, so that a program printing numbers does not
 * carry them.
 *
 * Nothing is buffered. MOS writes a character at a time and the console is
 * the only destination there is, so a buffer would be bytes held back for no
 * one's benefit.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* acc's own runtime, where the instruction that asks MOS for a character
 * lives. The same one putchar goes through. */
int acc_rt_putch(int c);

/* How many characters have gone out, which is what printf answers with. */
static int written;

/* Where they go. The screen when none of these is set, which is what printf
 * wants; a buffer with a size for snprintf; and a file for fprintf, reached
 * through a routine this file is handed rather than one it names.
 *
 * Handed, because naming fputc here would put the whole of the file layer
 * into every program that prints a string -- a library member is taken or
 * left whole, and printf is in almost everything. So the file routines set
 * these when they call the formatter, and a program that never opens a file
 * never links one. test/lib.sh measures exactly that. */
FILE  *acc_sink_file;
int  (*acc_sink_putc)(int, FILE *);
char  *acc_sink_buf;
size_t acc_sink_cap, acc_sink_at;
int  (*acc_sink_fn)(int);
extern int (*__acc_stdout_putc)(int c);

static void emit(int c)
{
    if (acc_sink_buf) {
        /* Counted whether it fits or not: snprintf answers with the length
         * the whole thing would have been, which is what lets a caller ask
         * with no buffer at all and then allocate. */
        if (acc_sink_at + 1 < acc_sink_cap)
            acc_sink_buf[acc_sink_at] = (char) c;
        acc_sink_at++;
    } else if (acc_sink_fn) {
        acc_sink_fn(c);                 /* the wide printf: see wprintf.c */
    } else if (acc_sink_file) {
        acc_sink_putc(c, acc_sink_file);
    } else if (__acc_stdout_putc) {
        __acc_stdout_putc(c);           /* stdout is a file: see stdio.c */
    } else {
        acc_rt_putch(c);
    }
    written++;
}

static void emit_run(const char *s, int len)
{
    while (len-- > 0)
        emit(*s++);
}

static void emit_fill(int n, int c)
{
    while (n-- > 0)
        emit(c);
}

/* The flags a conversion was written with, gathered so that the one place
 * that lays a field out can have all of them. */
enum {
    FL_LEFT  = 1,               /* `-`: against the left of the field */
    FL_ZERO  = 2,               /* `0`: filled with zeros, not blanks */
    FL_PLUS  = 4,               /* `+`: a sign even when it is positive */
    FL_SPACE = 8,               /* ` `: a blank where that sign would be */
    FL_ALT   = 16               /* `#`: octal led by a 0, hex by 0x */
};

/* A wide character as UTF-8, into `out`: one byte to three, which is as
 * far as a wchar_t of sixteen bits goes. Answers how many. */
static int utf8(unsigned c, char *out)
{
    c &= 0xffff;
    if (c < 0x80) {
        out[0] = (char) c;

        return 1;
    }
    if (c < 0x800) {
        out[0] = (char) (0xc0 | c >> 6);
        out[1] = (char) (0x80 | (c & 0x3f));

        return 2;
    }
    out[0] = (char) (0xe0 | c >> 12);
    out[1] = (char) (0x80 | ((c >> 6) & 0x3f));
    out[2] = (char) (0x80 | (c & 0x3f));

    return 3;
}

/* %ls: the wide string as UTF-8, with the precision the most bytes to
 * write and never part of a character, as C99 7.19.6.1 says, and the width
 * counted in bytes too. */
static void wide_string(const wchar_t *s, int width, int prec, int flags)
{
    char b[3];
    int len = 0, blanks, i, n;

    if (!s)
        s = L"(null)";
    for (i = 0; s[i]; i++) {
        n = utf8((unsigned) s[i], b);
        if (prec >= 0 && len + n > prec)
            break;
        len += n;
    }
    blanks = width > len ? width - len : 0;
    if (!(flags & FL_LEFT))
        emit_fill(blanks, ' ');
    while (i-- > 0)
        emit_run(b, utf8((unsigned) *s++, b));
    if (flags & FL_LEFT)
        emit_fill(blanks, ' ');
}

/* The digits of an unsigned value, in the base asked for, written backwards
 * into `buf` and answered as a length. Backwards because that is the order
 * the divisions produce them in; the caller reads the buffer from its end.
 *
 * The value is a long long whatever the conversion was: the widening is one
 * move at the call, and the alternative is this routine three times over. */
#define DIGITS_MAX  24          /* a 64-bit octal, and room to spare */

static int digits_of(unsigned long long v, unsigned base, int upper, char *buf)
{
    const char *alphabet = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int len = 0;

    do {
        buf[len++] = alphabet[v % base];
        v /= base;
    } while (v);

    return len;
}

/* One converted field: its digits (backwards, from digits_of), whatever goes
 * in front of them -- a sign, or the `0x` of a pointer -- and the width and
 * precision the conversion asked for.
 *
 * A precision is a count of digits: it fills with zeros up to it, and it
 * turns the `0` flag off, which is what C says. A width is a count of
 * characters: it fills with blanks, or with zeros between the prefix and the
 * digits when `0` is still on. */
static void emit_number(const char *digits, int ndigits, const char *prefix,
                        int nprefix, int width, int prec, int flags)
{
    int zeros = prec > ndigits ? prec - ndigits : 0;
    int len = nprefix + zeros + ndigits;
    int blanks = width > len ? width - len : 0;

    if (prec >= 0)
        flags &= ~FL_ZERO;
    if (blanks && (flags & FL_ZERO) && !(flags & FL_LEFT)) {
        zeros += blanks;
        blanks = 0;
    }
    if (!(flags & FL_LEFT))
        emit_fill(blanks, ' ');
    emit_run(prefix, nprefix);
    emit_fill(zeros, '0');
    while (ndigits-- > 0)
        emit(digits[ndigits]);
    if (flags & FL_LEFT)
        emit_fill(blanks, ' ');
}

/* A number as read from the format: a width or a precision written out. */
static int format_number(const char **pp)
{
    int n = 0;

    while (**pp >= '0' && **pp <= '9')
        n = n * 10 + *(*pp)++ - '0';

    return n;
}

/* The floating conversions, which are nine kilobytes and in a file of
 * their own: a program that prints no float should not carry them. printf
 * holds their address through a weak reference, which a link does not go
 * looking for on its own account; a file that passes a float or a double
 * to a function taking `...` asks for them (see gen_want), and then the
 * link takes them from the library and this is their address. Without
 * that it is zero. Read through a variable, so that it is asked at run
 * time and not assumed. */
#pragma weak acc_format_float
int acc_format_float(int c, int flags, int prec, double value,
                     const char **text, int *zeros_at);

static int (*float_hook)(int, int, int, double, const char **, int *)
    = acc_format_float;

/* The whole of the formatting, over a list someone else has started. Named
 * rather than static so that the file routines can use it without this file
 * having to know about them. */
int acc_format(const char *fmt, va_list ap)
{
    char digits[DIGITS_MAX];
    const char *p = fmt;

    written = 0;
    while (*p) {
        int flags = 0, width = 0, prec = -1, longs = 0, shorts = 0, upper = 0;
        unsigned long long value;
        unsigned base = 10;
        char prefix[2];
        int nprefix = 0, ndigits;
        int negative = 0;
        int c;

        if (*p != '%') {
            emit(*p++);

            continue;
        }
        p++;

        for (;; p++) {
            if (*p == '-')      flags |= FL_LEFT;
            else if (*p == '0') flags |= FL_ZERO;
            else if (*p == '+') flags |= FL_PLUS;
            else if (*p == ' ') flags |= FL_SPACE;
            else if (*p == '#') flags |= FL_ALT;
            else                break;
        }

        if (*p == '*') {
            p++;
            width = va_arg(ap, int);
            if (width < 0) {    /* a negative width is the `-` flag */
                flags |= FL_LEFT;
                width = -width;
            }
        } else {
            width = format_number(&p);
        }

        if (*p == '.') {
            p++;
            if (*p == '*') {
                p++;
                prec = va_arg(ap, int);
                if (prec < 0)
                    prec = -1;  /* as if it had not been written */
            } else {
                prec = format_number(&p);
            }
        }

        /* The lengths. `h` and `hh` read an int, which is what anything
         * narrower arrives as, and cut the value down to a short or a char
         * below. `j` is an intmax_t, a long long here; `z` and `t` are an
         * unsigned int and an int, which a plain conversion already reads.
         * `L` is a long double, which acc does not have. */
        for (;;) {
            if (*p == 'l')      { longs++; p++; }
            else if (*p == 'h') { shorts++; p++; }
            else if (*p == 'j') { longs = 2; p++; }
            else if (*p == 'z' || *p == 't' || *p == 'L') { p++; }
            else                break;
        }

        c = *p++;
        switch (c) {
        case 'd':
        case 'i': {
            long long signed_value = longs > 1 ? va_arg(ap, long long)
                                   : longs     ? (long long) va_arg(ap, long)
                                               : (long long) va_arg(ap, int);

            if (shorts == 1)
                signed_value = (short) signed_value;
            else if (shorts > 1)
                signed_value = (signed char) signed_value;

            negative = signed_value < 0;
            value = negative ? (unsigned long long) -signed_value
                             : (unsigned long long) signed_value;
            if (negative)
                prefix[nprefix++] = '-';
            else if (flags & FL_PLUS)
                prefix[nprefix++] = '+';
            else if (flags & FL_SPACE)
                prefix[nprefix++] = ' ';
            break;
        }
        case 'o':
        case 'u':
        case 'x':
        case 'X':
            base = c == 'o' ? 8u : c == 'u' ? 10u : 16u;
            upper = c == 'X';
            value = longs > 1 ? va_arg(ap, unsigned long long)
                  : longs     ? (unsigned long long) va_arg(ap, unsigned long)
                              : (unsigned long long) va_arg(ap, unsigned int);
            if (shorts == 1)
                value = (unsigned short) value;
            else if (shorts > 1)
                value = (unsigned char) value;
            /* `#` on a hex that is not zero: the `0x` in front of it. The
             * octal's leading zero is a digit rather than a prefix -- C
             * counts it among them -- so it is put on below, where the
             * digits are. */
            if ((flags & FL_ALT) && base == 16u && value) {
                prefix[nprefix++] = '0';
                prefix[nprefix++] = upper ? 'X' : 'x';
            }
            break;
        case 'p':
            /* An address, as `0x` and the hex of it. */
            base = 16u;
            value = (unsigned long long) (unsigned int) va_arg(ap, void *);
            prefix[nprefix++] = '0';
            prefix[nprefix++] = 'x';
            break;
        case 'c': {
            char one[3], t;
            int n = 1;

            /* %lc is a wide character, written as the bytes of its UTF-8 --
             * backwards, since emit_number writes digits from the end. */
            if (longs) {
                n = utf8((unsigned) va_arg(ap, int), one);
                t = one[0];
                one[0] = one[n - 1];
                one[n - 1] = t;
            } else {
                one[0] = (char) va_arg(ap, int);
            }
            emit_number(one, n, prefix, 0, width, -1, flags & FL_LEFT);

            continue;
        }
        case 's': {
            const char *s;
            int len = 0;
            int blanks;

            if (longs) {
                wide_string(va_arg(ap, const wchar_t *), width, prec, flags);

                continue;
            }
            s = va_arg(ap, const char *);
            if (!s)
                s = "(null)";
            /* A precision on a string is the most of it to write, and it
             * may run past the end of what is there -- so the length is
             * measured no further than that rather than with strlen, which
             * would read bytes the caller never promised. */
            while (s[len] && (prec < 0 || len < prec))
                len++;
            blanks = width > len ? width - len : 0;
            if (!(flags & FL_LEFT))
                emit_fill(blanks, ' ');
            emit_run(s, len);
            if (flags & FL_LEFT)
                emit_fill(blanks, ' ');

            continue;
        }
        case 'f': case 'F': case 'e': case 'E':
        case 'g': case 'G': case 'a': case 'A': {
            double v = va_arg(ap, double);
            const char *text;
            int len, zeros_at, pad;

            /* Not linked, so nothing in the program passed a float to a
             * function that takes more than it names -- and so nothing
             * could have come here with one. Said as a question mark
             * rather than as nothing. */
            if (!float_hook) {
                emit('?');

                continue;
            }
            len = float_hook(c, flags, prec, v, &text, &zeros_at);
            pad = width > len ? width - len : 0;
            if ((flags & FL_ZERO) && !(flags & FL_LEFT) && zeros_at >= 0) {
                emit_run(text, zeros_at);
                emit_fill(pad, '0');
                emit_run(text + zeros_at, len - zeros_at);

                continue;
            }
            if (!(flags & FL_LEFT))
                emit_fill(pad, ' ');
            emit_run(text, len);
            if (flags & FL_LEFT)
                emit_fill(pad, ' ');

            continue;
        }
        case 'n': {
            /* How much has been written so far, into what the argument
             * points at, as wide as the length says. */
            void *to = va_arg(ap, void *);

            if (longs > 1)
                *(long long *) to = written;
            else if (longs)
                *(long *) to = written;
            else if (shorts == 1)
                *(short *) to = (short) written;
            else if (shorts > 1)
                *(signed char *) to = (signed char) written;
            else
                *(int *) to = written;

            continue;
        }
        case '%':
            emit('%');

            continue;
        case '\0':
            /* A `%` last in the format: there is nothing it can convert, so
             * it is written as it stands and the walk ends. */
            emit('%');
            p--;

            continue;
        default:
            /* Something this printf does not know. Written out as it was
             * given, which is more use than swallowing it. */
            emit('%');
            emit(c);

            continue;
        }

        ndigits = digits_of(value, base, upper, digits);
        /* A zero written with a precision of zero is nothing at all. */
        if (prec == 0 && !value)
            ndigits = 0;
        /* `%#o`, whose leading zero is one of the digits: added only when
         * there is not one there already, and not when the precision is
         * going to put one there. */
        if ((flags & FL_ALT) && base == 8u
            && (ndigits == 0
                || (digits[ndigits - 1] != '0' && prec <= ndigits)))
            digits[ndigits++] = '0';
        emit_number(digits, ndigits, prefix, nprefix, width, prec, flags);
    }
    return written;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    int n;

    acc_sink_file = NULL;
    acc_sink_buf = NULL;
    va_start(ap, fmt);
    n = acc_format(fmt, ap);
    va_end(ap);

    return n;
}

int vsnprintf(char *to, size_t n, const char *fmt, va_list ap)
{
    int wanted;

    acc_sink_file = NULL;
    acc_sink_buf = to;
    acc_sink_cap = n;
    acc_sink_at = 0;
    wanted = acc_format(fmt, ap);
    if (n)
        to[acc_sink_at < n ? acc_sink_at : n - 1] = '\0';
    acc_sink_buf = NULL;

    return wanted;
}

int snprintf(char *to, size_t n, const char *fmt, ...)
{
    va_list ap;
    int wanted;

    va_start(ap, fmt);
    wanted = vsnprintf(to, n, fmt, ap);
    va_end(ap);

    return wanted;
}

/* sprintf is snprintf told the buffer has no end. C99 asks the caller to
 * have made it big enough, and there is no size to check against -- which
 * is why snprintf is the one to use, and why sprintf is still C99. */
int vsprintf(char *to, const char *fmt, va_list ap)
{
    return vsnprintf(to, (size_t) -1, fmt, ap);
}

int sprintf(char *to, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(to, (size_t) -1, fmt, ap);
    va_end(ap);

    return n;
}

int vprintf(const char *fmt, va_list ap)
{
    acc_sink_file = NULL;
    acc_sink_buf = NULL;

    return acc_format(fmt, ap);
}
