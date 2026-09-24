/*
 * strtod, strtof and atof: a floating number at the front of a string.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Correctly rounded: the answer is the float nearest the decimal written,
 * ties to even, however many digits it was written with. That is done
 * exactly, in whole numbers. The digits are an integer M and a power of
 * ten E, so the value is M * 10^E, or M / 10^-E; the quotient is taken to
 * twenty-five bits -- the twenty-four a float keeps and one more to round
 * by -- with whether anything was left over, and the rounding happens once,
 * on that. The integers are kept as 16-bit limbs, and are as large as a
 * denormal's exact value needs.
 *
 * Only the first 120 significant digits are kept. That is enough: a number
 * exactly halfway between two floats has at most 112 of them, so digits past
 * the 120th can only say which side of such a point the number is on, and
 * one more 1 at the end, standing for all of them, says the same.
 *
 * Hexadecimal floats, 0x1.8p3, are read exactly too, and so are INF,
 * INFINITY and NAN, in either case. The same reader serves the wide
 * strings of <wchar.h>, as __acc_strtou does for integers.
 */
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#define LIMBS   40              /* 640 bits: past what a denormal takes */
#define DIGITS  120

typedef struct {
    int      n;                 /* limbs in use */
    uint16_t v[LIMBS];
} Big;

static Big num, den;

static void big_set(Big *b, unsigned v)
{
    b->n = v != 0;
    b->v[0] = (uint16_t) v;
}

/* b = b * m + a, for small m and a. */
static void big_mul_add(Big *b, unsigned m, unsigned a)
{
    unsigned long carry = a;
    int i;

    for (i = 0; i < b->n; i++) {
        carry += (unsigned long) b->v[i] * m;
        b->v[i] = (uint16_t) carry;
        carry >>= 16;
    }
    if (carry && b->n < LIMBS)
        b->v[b->n++] = (uint16_t) carry;
}

static int big_bits(const Big *b)
{
    int bits = (b->n - 1) * 16;
    unsigned top = b->v[b->n - 1];

    while (top) {
        bits++;
        top >>= 1;
    }

    return bits;
}

static void big_shl(Big *b, int s)
{
    int limbs = s / 16, bits = s % 16, i;

    if (bits) {
        unsigned carry = 0;

        for (i = 0; i < b->n; i++) {
            unsigned long t = ((unsigned long) b->v[i] << bits) | carry;

            b->v[i] = (uint16_t) t;
            carry = (unsigned) (t >> 16);
        }
        if (carry)
            b->v[b->n++] = (uint16_t) carry;
    }
    if (limbs) {
        for (i = b->n - 1; i >= 0; i--)
            b->v[i + limbs] = b->v[i];
        for (i = 0; i < limbs; i++)
            b->v[i] = 0;
        b->n += limbs;
    }
}

static void big_shr1(Big *b)
{
    int i;

    for (i = 0; i < b->n; i++) {
        b->v[i] >>= 1;
        if (i + 1 < b->n && (b->v[i + 1] & 1))
            b->v[i] |= 0x8000;
    }
    if (b->n && !b->v[b->n - 1])
        b->n--;
}

static int big_cmp(const Big *a, const Big *b)
{
    int i;

    if (a->n != b->n)
        return a->n < b->n ? -1 : 1;
    for (i = a->n - 1; i >= 0; i--)
        if (a->v[i] != b->v[i])
            return a->v[i] < b->v[i] ? -1 : 1;

    return 0;
}

static void big_sub(Big *a, const Big *b)
{
    long borrow = 0;
    int i;

    for (i = 0; i < a->n; i++) {
        long t = (long) a->v[i] - (i < b->n ? b->v[i] : 0) - borrow;

        borrow = t < 0;
        a->v[i] = (uint16_t) (t + (borrow ? 65536L : 0));
    }
    while (a->n && !a->v[a->n - 1])
        a->n--;
}

/* m * 2^e, rounded once to a float, where `sticky` says something below m
 * was not zero. */
static double finish(unsigned long long m, int e, int sticky, int negative)
{
    double v;

    while (m >= (1ULL << 25)) {
        sticky |= (int) (m & 1);
        m >>= 1;
        e++;
    }
    while (m < (1ULL << 24)) {
        m <<= 1;
        e--;
    }

    /* Among the denormals the last bit kept is worth 2^-149, and the one
     * below it, which rounds, 2^-150. */
    while (e < -150 && m) {
        sticky |= (int) (m & 1);
        m >>= 1;
        e++;
    }
    if (e < -150)
        e = -150;

    {
        int guard = (int) (m & 1);

        m >>= 1;
        e++;
        if (guard && (sticky || (m & 1)))
            m++;
        sticky |= guard;        /* from here, whether it was inexact */
    }

    if (e + 23 > 127 || (m >= (1ULL << 24) && e + 24 > 127)) {
        errno = ERANGE;
        v = HUGE_VAL;
    } else {
        v = scalbn((double) (unsigned long) m, e);
        /* Underflow is a denormal or a zero that is not exact, as glibc
         * has it. */
        if (sticky && (m < (1ULL << 23) || v == 0.0f))
            errno = ERANGE;
    }

    return negative ? -v : v;
}

#define AT(p) (step == 1 ? *(const unsigned char *) (p) \
                         : *(const unsigned short *) (p))

static int lower_is(const char *s, const char *word, int step)
{
    for (; *word; word++, s += step)
        if (tolower(AT(s)) != *word)
            return 0;

    return 1;
}

static int hex_digit(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

/* An exponent after the digits, if one is there and complete: `e` or `p`,
 * a sign, and at least one digit. Left alone otherwise, as C says. */
static const char *exponent(const char *s, int step, long *e)
{
    const char *p = s + step;
    int negative = 0;
    long v = 0;

    if (AT(p) == '+' || AT(p) == '-') {
        negative = AT(p) == '-';
        p += step;
    }
    if (AT(p) < '0' || AT(p) > '9')
        return s;
    for (; AT(p) >= '0' && AT(p) <= '9'; p += step)
        if (v < 100000L)
            v = v * 10 + (AT(p) - '0');
    *e = negative ? -v : v;

    return p;
}

/* The digits after 0x, into *v; 0 when there are none, and then the number
 * is the 0 in front of the x. */
static int hex(const char *s, const char **end, int step, int negative,
               double *v)
{
    unsigned long long m = 0;
    int e = 0, sticky = 0, any = 0, d;
    long p = 0;

    for (; (d = hex_digit(AT(s))) >= 0; s += step, any = 1) {
        if (m < (1ULL << 56))
            m = m * 16 + (unsigned) d;
        else {
            sticky |= d != 0;
            e += 4;
        }
    }
    if (AT(s) == '.') {
        s += step;
        for (; (d = hex_digit(AT(s))) >= 0; s += step, any = 1) {
            if (m < (1ULL << 56)) {
                m = m * 16 + (unsigned) d;
                e -= 4;
            } else {
                sticky |= d != 0;
            }
        }
    }
    if (!any)
        return 0;
    if (AT(s) == 'p' || AT(s) == 'P')
        s = exponent(s, step, &p);
    *end = s;
    if (p > 1000)
        p = 1000;
    if (p < -1000)
        p = -1000;
    if (!m)
        *v = negative ? -0.0f : 0.0f;
    else
        *v = finish(m, e + (int) p, sticky, negative);

    return 1;
}

double __acc_strtod(const char *s, char **end, int step)
{
    static char digits[DIGITS];
    const char *start = s, *after;
    int negative = 0, nd = 0, sticky = 0, any = 0, i, shift;
    long e = 0, x = 0;
    unsigned long long q;

    while (AT(s) == ' ' || (AT(s) >= 9 && AT(s) <= 13))
        s += step;
    if (AT(s) == '+' || AT(s) == '-') {
        negative = AT(s) == '-';
        s += step;
    }

    if (lower_is(s, "inf", step)) {
        s += 3 * step;
        if (lower_is(s, "inity", step))
            s += 5 * step;
        if (end)
            *end = (char *) s;

        return negative ? -HUGE_VAL : HUGE_VAL;
    }
    if (lower_is(s, "nan", step)) {
        s += 3 * step;
        if (AT(s) == '(') {
            const char *p = s + step;

            while (isalnum(AT(p)) || AT(p) == '_')
                p += step;
            if (AT(p) == ')')
                s = p + step;
        }
        if (end)
            *end = (char *) s;

        return negative ? -NAN : NAN;
    }

    if (AT(s) == '0' && (AT(s + step) == 'x' || AT(s + step) == 'X')) {
        double v;

        if (!hex(s + 2 * step, &after, step, negative, &v)) {
            /* "0x" and nothing hex after it: the number is the 0. */
            if (end)
                *end = (char *) (s + step);

            return negative ? -0.0f : 0.0f;
        }
        if (end)
            *end = (char *) after;

        return v;
    }

    /* The digits, without their leading zeros, with the point counted in
     * the exponent. */
    for (;; s += step) {
        int c = AT(s);

        if (c == '.' && !x) {
            x = 1;
            continue;
        }
        if (c < '0' || c > '9')
            break;
        any = 1;
        if (c == '0' && !nd) {
            e -= x;
            continue;
        }
        if (nd < DIGITS) {
            digits[nd++] = (char) (c - '0');
            e -= x;
        } else {
            sticky |= c != '0';
            e += !x;
        }
    }
    if (!any) {
        if (end)
            *end = (char *) start;

        return 0.0f;
    }
    if (AT(s) == 'e' || AT(s) == 'E') {
        long p = 0;

        s = exponent(s, step, &p);
        e += p;
    }
    if (end)
        *end = (char *) s;
    if (!nd)
        return negative ? -0.0f : 0.0f;

    /* Out of range either way before any arithmetic: past 10^39 is past
     * the largest float, and below 10^-46 rounds to zero. */
    if (nd + e > 40) {
        errno = ERANGE;

        return negative ? -HUGE_VAL : HUGE_VAL;
    }
    if (nd + e < -46) {
        errno = ERANGE;

        return negative ? -0.0f : 0.0f;
    }

    big_set(&num, 0);
    for (i = 0; i < nd; i++)
        big_mul_add(&num, 10, (unsigned) digits[i]);
    if (sticky) {
        big_mul_add(&num, 10, 1);
        e--;
    }
    big_set(&den, 1);
    for (; e > 0; e--)
        big_mul_add(&num, 10, 0);
    for (; e < 0; e++)
        big_mul_add(&den, 10, 0);

    /* num / den to between 2^25 and 2^27, by scaling one of them by a
     * power of two, and the quotient a bit at a time. */
    shift = 26 - (big_bits(&num) - big_bits(&den));
    if (shift > 0)
        big_shl(&num, shift);
    else if (shift < 0)
        big_shl(&den, -shift);
    big_shl(&den, 26);
    q = 0;
    for (i = 0; i <= 26; i++) {
        q <<= 1;
        if (big_cmp(&num, &den) >= 0) {
            big_sub(&num, &den);
            q |= 1;
        }
        big_shr1(&den);
    }

    return finish(q, -shift, num.n != 0, negative);
}

double strtod(const char *s, char **end)
{
    return __acc_strtod(s, end, 1);
}

float strtof(const char *s, char **end)
{
    return (float) __acc_strtod(s, end, 1);
}

double atof(const char *s)
{
    return __acc_strtod(s, NULL, 1);
}
