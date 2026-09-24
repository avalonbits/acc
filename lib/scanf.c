/*
 * scanf and its family: sscanf, fscanf and the v forms of each.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * One reader for all of them, handed where its characters come from as a
 * pair of functions -- the next character, and one to give back -- so that
 * sscanf does not bring the file layer with it. C lets scanf give back one
 * character and no more, and this never needs to: an input item is read
 * while it could still be the start of something the conversion accepts,
 * and the one character that ends it is the one given back. So "1e+x"
 * read with %f takes the "1e+" and fails, as C says it does.
 *
 * The numbers are converted by the same functions strtol and strtod are
 * made of, from the characters the item was read into.
 *
 * The wide forms in <wchar.h> read through here as well: the format may be
 * wide, and a character that is past a byte is a character like any other.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

unsigned long long __acc_strtou(const char *s, char **end, int base, int step,
                                unsigned long long pos, unsigned long long neg,
                                int *negative);
double __acc_strtod(const char *s, char **end, int step);

/* Where the characters come from. */
typedef struct {
    int  (*get)(void *src);
    void (*unget)(int c, void *src);
    void  *src;
    int    count;               /* read and kept, which %n answers */
} Input;

static int next(Input *in)
{
    int c = in->get(in->src);

    if (c != EOF)
        in->count++;

    return c;
}

static void back(Input *in, int c)
{
    if (c == EOF)
        return;
    in->unget(c, in->src);
    in->count--;
}

static int is_space(int c)
{
    return c == ' ' || (c >= 9 && c <= 13);
}

static void skip_space(Input *in)
{
    int c;

    while (is_space(c = next(in)))
        ;
    back(in, c);
}

#define ITEM_MAX 128            /* the longest number read */

/* An integer's characters, into `buf`: a sign, a 0x where the base allows
 * one, and digits of the base. Answers how many were read. */
static int read_integer(Input *in, char *buf, int width, int base)
{
    int n = 0, c, digits = 0;

    if (width <= 0 || width > ITEM_MAX - 1)
        width = ITEM_MAX - 1;
    c = next(in);
    if ((c == '+' || c == '-') && n < width) {
        buf[n++] = (char) c;
        c = next(in);
    }
    if ((base == 0 || base == 16) && c == '0' && n < width) {
        buf[n++] = (char) c;
        digits = 1;
        c = next(in);
        if ((c == 'x' || c == 'X') && n < width) {
            /* "0x" with no hex digit after it is taken as the 0, as glibc
             * takes it: the x is gone, and there is no giving back two. */
            buf[n++] = (char) c;
            base = 16;
            c = next(in);
        } else if (base == 0) {
            base = 8;
        }
    }
    if (base == 0)
        base = 10;
    for (; n < width; c = next(in)) {
        int d = c >= '0' && c <= '9' ? c - '0'
              : c >= 'a' && c <= 'z' ? c - 'a' + 10
              : c >= 'A' && c <= 'Z' ? c - 'A' + 10 : 99;

        if (d >= base)
            break;
        buf[n++] = (char) c;
        digits++;
    }
    back(in, c);
    buf[n] = 0;

    return digits ? n : 0;
}

/* Whether `c` could come next in the letters of `word`, lower case. */
static int matches(int c, const char *word)
{
    return *word && tolower(c) == *word;
}

/* A floating number's characters, the way strtod reads them: a sign, then
 * INF or INFINITY, NAN with what may follow it in brackets, or digits of
 * either base with a point and an exponent. */
static int read_float(Input *in, char *buf, int width)
{
    int n = 0, c, digits = 0, hex = 0, point = 0;

    if (width <= 0 || width > ITEM_MAX - 1)
        width = ITEM_MAX - 1;
    c = next(in);
    if ((c == '+' || c == '-') && n < width) {
        buf[n++] = (char) c;
        c = next(in);
    }

    if (matches(c, "inf") || matches(c, "nan")) {
        const char *word = tolower(c) == 'i' ? "infinity" : "nan";
        int i;

        for (i = 0; word[i] && n < width && tolower(c) == word[i]; i++) {
            buf[n++] = (char) c;
            c = next(in);
        }
        if (word[0] == 'i' ? i != 3 && i != 8 : i != 3) {
            back(in, c);

            return 0;
        }
        if (word[0] == 'n' && c == '(' && n < width) {
            buf[n++] = (char) c;
            while ((c = next(in)) != EOF && n < width
                   && (isalnum(c) || c == '_'))
                buf[n++] = (char) c;
            if (c != ')' || n >= width) {
                back(in, c);

                return 0;
            }
            buf[n++] = (char) c;
            c = next(in);
        }
        back(in, c);
        buf[n] = 0;

        return n;
    }

    if (c == '0' && n < width) {
        buf[n++] = (char) c;
        digits = 1;
        c = next(in);
        if ((c == 'x' || c == 'X') && n < width) {
            buf[n++] = (char) c;
            digits = 0;
            hex = 1;
            c = next(in);
        }
    }
    for (; n < width; c = next(in)) {
        if (c == '.' && !point) {
            point = 1;
        } else if (hex ? isxdigit(c) : isdigit(c)) {
            digits++;
        } else {
            break;
        }
        buf[n++] = (char) c;
    }
    if (digits && n < width && (hex ? c == 'p' || c == 'P' : c == 'e' || c == 'E')) {
        int exp_digits = 0;

        buf[n++] = (char) c;
        c = next(in);
        if ((c == '+' || c == '-') && n < width) {
            buf[n++] = (char) c;
            c = next(in);
        }
        for (; n < width && isdigit(c); c = next(in)) {
            buf[n++] = (char) c;
            exp_digits++;
        }
        if (!exp_digits)
            digits = 0;         /* "1e" is no number, and C takes it anyway */
    }
    back(in, c);
    buf[n] = 0;

    return digits ? n : 0;
}

/* A scan set, [...], at `p`: what it accepts, into `set` as 256 flags.
 * Answers where the format goes on from. */
static const char *scan_set(const char *p, int fs, unsigned char *set)
{
    int invert = 0, c, i;

#define F(q) (fs == 1 ? *(const unsigned char *) (q) : *(const unsigned short *) (q))
    memset(set, 0, 256);
    if (F(p) == '^') {
        invert = 1;
        p += fs;
    }
    if (F(p) == ']') {
        set[']'] = 1;
        p += fs;
    }
    while ((c = F(p)) && c != ']') {
        if (F(p + fs) == '-' && F(p + 2 * fs) && F(p + 2 * fs) != ']') {
            int hi = F(p + 2 * fs);

            for (i = c; i <= hi && i < 256; i++)
                set[i] = 1;
            p += 3 * fs;
        } else {
            if (c < 256)
                set[c] = 1;
            p += fs;
        }
    }
    if (invert)
        for (i = 0; i < 256; i++)
            set[i] = !set[i];

    return F(p) ? p + fs : p;
#undef F
}

/* Where a %c, %s or %[ puts what it reads, which may be bytes or wide
 * characters, from input that may be either: bytes into wide characters
 * are UTF-8 decoded, and wide characters into bytes are UTF-8 encoded, as
 * mbrtowc and wcrtomb would do it. */
typedef struct {
    char         *p;            /* null when the item is not stored */
    int           wide;         /* it stores wchar_t */
    int           in_wide;      /* the input is wide characters */
    unsigned long acc;          /* a character being decoded */
    int           need;         /* and how many more bytes it wants */
} Sink;

static void put_wide(Sink *s, unsigned c)
{
    *(wchar_t *) s->p = (wchar_t) c;
    s->p += sizeof (wchar_t);
}

static void put(Sink *s, int c)
{
    unsigned u = (unsigned) c;

    if (!s->p)
        return;
    if (!s->in_wide) {
        if (!s->wide) {
            *s->p++ = (char) c;
        } else if (s->need) {
            s->acc = (s->acc << 6) | (u & 0x3f);
            if (!--s->need)
                put_wide(s, (unsigned) s->acc);
        } else if (u >= 0xc0 && u < 0xf0) {
            s->need = u >= 0xe0 ? 2 : 1;
            s->acc = u & (u >= 0xe0 ? 0x0fu : 0x1fu);
        } else {
            put_wide(s, u);
        }

        return;
    }
    u &= 0xffff;
    if (s->wide) {
        put_wide(s, u);
    } else if (u < 0x80) {
        *s->p++ = (char) u;
    } else if (u < 0x800) {
        *s->p++ = (char) (0xc0 | u >> 6);
        *s->p++ = (char) (0x80 | (u & 0x3f));
    } else {
        *s->p++ = (char) (0xe0 | u >> 12);
        *s->p++ = (char) (0x80 | ((u >> 6) & 0x3f));
        *s->p++ = (char) (0x80 | (u & 0x3f));
    }
}

static void store_integer(void *to, int longs, int shorts, unsigned long long v)
{
    if (longs > 1)
        *(long long *) to = (long long) v;
    else if (longs)
        *(long *) to = (long) v;
    else if (shorts > 1)
        *(signed char *) to = (signed char) v;
    else if (shorts)
        *(short *) to = (short) v;
    else
        *(int *) to = (int) v;
}

/* The reading. `fs` is how wide a format character is: 1, or 2 for the
 * wide functions, whose %s and %c store wide characters unless an h is
 * written. */
int __acc_scan(Input *in, const char *fmt, int fs, va_list ap)
{
    static unsigned char set[256];
    char buf[ITEM_MAX];
    int assigned = 0, converted = 0, input_failure = 0, c;

#define F(q) (fs == 1 ? *(const unsigned char *) (q) : *(const unsigned short *) (q))
    while (F(fmt)) {
        int suppress = 0, width = 0, longs = 0, shorts = 0, conv, base = 10;
        void *to = NULL;

        if (is_space(F(fmt))) {
            while (is_space(F(fmt)))
                fmt += fs;
            skip_space(in);
            continue;
        }
        if (F(fmt) != '%' || F(fmt + fs) == '%') {
            if (F(fmt) == '%') {
                fmt += fs;
                skip_space(in);
            }
            c = next(in);
            if (c != F(fmt)) {
                input_failure = c == EOF;
                back(in, c);
                break;
            }
            fmt += fs;
            continue;
        }

        fmt += fs;
        if (F(fmt) == '*') {
            suppress = 1;
            fmt += fs;
        }
        while (F(fmt) >= '0' && F(fmt) <= '9') {
            width = width * 10 + (F(fmt) - '0');
            fmt += fs;
        }
        for (;; fmt += fs) {
            conv = F(fmt);
            if (conv == 'l')      longs++;
            else if (conv == 'h') shorts++;
            else if (conv == 'j') longs = 2;
            else if (conv == 'L') longs = 1;
            else if (conv != 'z' && conv != 't') break;
        }
        if (!conv)
            break;
        fmt += fs;
        if (!suppress && conv != '%')
            to = va_arg(ap, void *);

        if (conv != '[' && conv != 'c' && conv != 'n')
            skip_space(in);

        /* Nothing left to read an item from is an input failure, and the
         * only kind that makes the answer EOF; an item that is there and is
         * not a number, or not long enough, is a matching failure. See
         * C99 7.19.6.2p10. */
        if (conv != 'n') {
            c = next(in);
            if (c == EOF) {
                input_failure = 1;
                goto done;
            }
            back(in, c);
        }

        switch (conv) {
        case 'n':
            if (to)
                store_integer(to, longs, shorts, (unsigned long long) in->count);
            continue;

        case 'd': case 'i': case 'o': case 'u': case 'x': case 'X': case 'p': {
            int negative;
            unsigned long long v;

            base = conv == 'i' ? 0 : conv == 'o' ? 8
                 : conv == 'x' || conv == 'X' || conv == 'p' ? 16 : 10;
            if (!read_integer(in, buf, width, base))
                goto done;
            v = __acc_strtou(buf, NULL, base, 1, ~0ULL, ~0ULL, &negative);
            if (negative)
                v = 0 - v;
            if (to) {
                if (conv == 'p')
                    *(void **) to = (void *) (unsigned) v;
                else
                    store_integer(to, longs, shorts, v);
                assigned++;
            }
            converted++;
            continue;
        }

        case 'a': case 'A': case 'e': case 'E': case 'f': case 'F':
        case 'g': case 'G': {
            double v;

            if (!read_float(in, buf, width))
                goto done;
            v = __acc_strtod(buf, NULL, 1);
            if (to) {
                if (longs)
                    *(double *) to = v;
                else
                    *(float *) to = (float) v;
                assigned++;
            }
            converted++;
            continue;
        }

        case 'c': case 's': case '[': {
            /* A wide conversion is one with an l in the narrow functions,
             * or with no h in the wide ones. */
            Sink sink;
            int n = 0;

            sink.p = to;
            sink.wide = fs == 1 ? longs > 0 : shorts == 0;
            sink.in_wide = fs != 1;
            sink.need = 0;
            if (conv == '[')
                fmt = scan_set(fmt, fs, set);
            if (width <= 0)
                width = conv == 'c' ? 1 : -1;
            while (width < 0 || n < width) {
                c = next(in);
                if (c == EOF)
                    break;
                if (conv == 's' ? is_space(c)
                    : conv == '[' ? c >= 256 || !set[c] : 0) {
                    back(in, c);
                    break;
                }
                put(&sink, c);
                n++;
            }
            if (!n || (conv == 'c' && n < width))
                goto done;
            if (conv != 'c')
                put(&sink, 0);
            if (to)
                assigned++;
            converted++;
            continue;
        }

        default:
            goto done;
        }
    }
done:
#undef F
    if (!converted && input_failure)
        return EOF;

    return assigned;
}

/* From a string. */
static int string_get(void *src)
{
    const unsigned char **p = src;

    if (!**p)
        return EOF;

    return *(*p)++;
}

static void string_unget(int c, void *src)
{
    const unsigned char **p = src;

    (void) c;
    --*p;
}

int vsscanf(const char *s, const char *fmt, va_list ap)
{
    Input in;

    in.get = string_get;
    in.unget = string_unget;
    in.src = &s;
    in.count = 0;

    return __acc_scan(&in, fmt, 1, ap);
}

int sscanf(const char *s, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsscanf(s, fmt, ap);
    va_end(ap);

    return n;
}

/* From a stream. */
static int stream_get(void *src)
{
    return fgetc(src);
}

static void stream_unget(int c, void *src)
{
    ungetc(c, src);
}

int vfscanf(FILE *f, const char *fmt, va_list ap)
{
    Input in;

    in.get = stream_get;
    in.unget = stream_unget;
    in.src = f;
    in.count = 0;

    return __acc_scan(&in, fmt, 1, ap);
}

int fscanf(FILE *f, const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vfscanf(f, fmt, ap);
    va_end(ap);

    return n;
}

int vscanf(const char *fmt, va_list ap)
{
    return vfscanf(stdin, fmt, ap);
}

int scanf(const char *fmt, ...)
{
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vfscanf(stdin, fmt, ap);
    va_end(ap);

    return n;
}
