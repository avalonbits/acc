/*
 * printf's floating conversions: %f, %e, %g and %a, and their capitals.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * A file of their own because printf reaches them through a weak reference
 * -- see acc_format in lib/printf.c -- and a weak reference is to another
 * file: a program that prints no floats carries none of this, which is
 * nine kilobytes, most of the formatter it would otherwise be put in.
 */

#include <stdio.h>
#include <string.h>

/* The flags printf read, as it numbers them. */
#define FL_PLUS   4
#define FL_SPACE  8
#define FL_ALT   16

/* A float is m * 2^e with m 24 bits wide, so its value written in decimal
 * ends: at most 39 digits before the point and 149 after it. They are
 * worked out exactly, here, and then rounded where the conversion asks --
 * to the nearest, a tie to even -- which is what the C library on the
 * machine running the tests does with the same value, and what
 * test/printf.sh holds this to. No float arithmetic is done: the value is
 * taken apart from its bits, and the rest is integers.
 *
 * The digits are a static buffer, not a local: two hundred bytes of frame
 * are a stack this machine may not have to spare. */
#define FDIGITS_MAX 200

static char fdig[FDIGITS_MAX + 2];
static int  fdig_n, fdig_int;          /* digits in all, and before the point */

/* Sixteen bits a limb, lowest first: 160 bits hold the widest integer part
 * a float has, 2^128, and the longest fraction, 2^-149. */
#define LIMBS 10

static void limb_set_bit(unsigned *l, int bit)
{
    l[bit >> 4] |= 1u << (bit & 15);
}

/* All of a float's digits, into fdig: the integer part, "0" if it has none,
 * then every digit of the fraction until it runs out. */
static void float_digits(unsigned long bits)
{
    static unsigned limb[LIMBS];
    int ex = (int) ((bits >> 23) & 0xff), e, i, n = 0, any;
    unsigned m = (unsigned) (bits & 0x7fffffUL), t;
    char *d = fdig;

    if (ex) {
        m |= 0x800000u;
        e = ex - 150;
    } else {
        e = -149;
    }

    /* The integer part: the bits of m at or above 2^0, as a number of
     * limbs, divided by ten until nothing is left -- its digits come out
     * last first. */
    for (i = 0; i < LIMBS; i++)
        limb[i] = 0;
    /* m's bits from the top, each tested with a constant and moved up by a
     * shift left, which is add hl, hl: a variable shift or a variable AND is
     * a call. */
    for (i = 23, t = m; i >= 0; i--, t <<= 1)
        if (t & 0x800000u && i + e >= 0)
            limb_set_bit(limb, i + e);
    /* Both loops work in unsigned int, which is 24 bits here: a remainder
     * under ten above a limb, and a limb times ten plus a carry under ten,
     * are both under 655360, so neither needs a long -- and a long's
     * multiply and divide are several times an int's. */
    do {
        unsigned rem = 0;

        any = 0;
        for (i = LIMBS - 1; i >= 0; i--) {
            unsigned cur = rem << 16 | limb[i];
            unsigned q = cur / 10;

            limb[i] = q;
            rem = cur - q * 10;
            if (q)
                any = 1;
        }
        d[n++] = (char) ('0' + rem);
    } while (any);
    for (i = 0; i < n / 2; i++) {
        char t = d[i];

        d[i] = d[n - 1 - i];
        d[n - 1 - i] = t;
    }
    fdig_int = n;

    /* The fraction: the bits of m below 2^0, set at the top of the limbs,
     * so that multiplying by ten carries the next digit out of the top. It
     * ends, because a fraction of a power of two always does. */
    for (i = 0; i < LIMBS; i++)
        limb[i] = 0;
    for (i = 23, t = m; i >= 0; i--, t <<= 1)
        if (t & 0x800000u && i + e < 0)
            limb_set_bit(limb, LIMBS * 16 + i + e);
    for (;;) {
        unsigned carry = 0;

        any = 0;
        for (i = 0; i < LIMBS; i++) {
            unsigned cur = limb[i] * 10 + carry;

            limb[i] = cur & 0xffff;
            carry = cur >> 16;
            if (limb[i])
                any = 1;
        }
        if (!any && !carry)
            break;
        d[n++] = (char) ('0' + carry);
        if (!any)
            break;
    }
    fdig_n = n;
}

/* Keep the first `keep` digits, rounded to the nearest with a tie to even,
 * padded with zeros when there are fewer. Answers whether the rounding
 * carried out of the first, which makes a new leading digit. */
static int float_round(int keep)
{
    int i, up = 0;

    if (keep < 0)
        keep = 0;
    if (keep >= fdig_n) {
        while (fdig_n < keep && fdig_n < FDIGITS_MAX)
            fdig[fdig_n++] = '0';

        return 0;
    }
    if (fdig[keep] > '5') {
        up = 1;
    } else if (fdig[keep] == '5') {
        for (i = keep + 1; i < fdig_n && fdig[i] == '0'; i++)
            ;
        up = i < fdig_n || (keep > 0 && ((fdig[keep - 1] - '0') & 1));
    }
    fdig_n = keep;
    if (!up)
        return 0;
    for (i = keep - 1; i >= 0; i--) {
        if (fdig[i] != '9') {
            fdig[i]++;

            return 0;
        }
        fdig[i] = '0';
    }
    for (i = fdig_n; i > 0; i--)
        fdig[i] = fdig[i - 1];
    fdig[0] = '1';
    fdig_n++;

    return 1;
}

/* The text of one conversion, sign aside, and the sign. */
static char ftext[FDIGITS_MAX + 64];
static char fsign[1];

static int put_exponent(char *out, int x, int letter)
{
    int n = 0;

    out[n++] = (char) letter;
    out[n++] = x < 0 ? '-' : '+';
    if (x < 0)
        x = -x;
    if (x >= 100)
        out[n++] = (char) ('0' + x / 100);
    out[n++] = (char) ('0' + x / 10 % 10);
    out[n++] = (char) ('0' + x % 10);

    return n;
}

static int fixed_text(unsigned long bits, int prec, int alt)
{
    int n = 0, i;

    float_digits(bits);
    fdig_int += float_round(fdig_int + prec);
    for (i = 0; i < fdig_int; i++)
        ftext[n++] = fdig[i];
    if (prec > 0 || alt)
        ftext[n++] = '.';
    for (i = 0; i < prec; i++)
        ftext[n++] = fdig[fdig_int + i];

    return n;
}

/* The first digit that is not zero, and so the exponent in %e: -1 when the
 * value is zero. */
static int first_digit(void)
{
    int z;

    for (z = 0; z < fdig_n && fdig[z] == '0'; z++)
        ;

    return z < fdig_n ? z : -1;
}

static int exp_text(unsigned long bits, int prec, int alt, int upper, int *xp)
{
    int n = 0, i, z, x;

    float_digits(bits);
    z = first_digit();
    if (z < 0) {
        z = 0;
        x = 0;
        float_round(prec + 1);
    } else {
        /* Rounding can carry into the zero in front of the first digit --
         * 0.0999 to one place is 0.10 -- or out of the front altogether,
         * so where the first digit is is asked again after it. */
        if (float_round(z + prec + 1))
            fdig_int++;
        z = first_digit();
        x = fdig_int - 1 - z;
    }
    ftext[n++] = fdig[z];
    if (prec > 0 || alt)
        ftext[n++] = '.';
    for (i = 1; i <= prec; i++)
        ftext[n++] = fdig[z + i];
    n += put_exponent(ftext + n, x, upper ? 'E' : 'e');
    if (xp)
        *xp = x;

    return n;
}

/* %g: %e or %f, whichever C99 7.19.6.1 says the exponent calls for, with
 * the trailing zeros taken off unless `#` keeps them. */
static int general_text(unsigned long bits, int prec, int alt, int upper)
{
    int x, n, dot, i, e_at;

    if (prec == 0)
        prec = 1;
    exp_text(bits, prec - 1, 0, upper, &x);
    if (x >= -4 && x < prec)
        n = fixed_text(bits, prec - 1 - x, alt);
    else
        n = exp_text(bits, prec - 1, alt, upper, NULL);
    if (alt)
        return n;

    for (dot = 0; dot < n && ftext[dot] != '.'; dot++)
        ;
    if (dot == n)
        return n;
    for (e_at = dot; e_at < n && ftext[e_at] != 'e' && ftext[e_at] != 'E'; e_at++)
        ;
    for (i = e_at; i > dot + 1 && ftext[i - 1] == '0'; i--)
        ;
    if (i == dot + 1)
        i = dot;
    memmove(ftext + i, ftext + e_at, (size_t) (n - e_at));

    return i + (n - e_at);
}

/* %a: the value in hex, as 1.hhh and a power of two, which is exact --
 * a subnormal made normal first, as the C library does with a double,
 * which every float is as one. Without a precision, no trailing zeros. */
static int hex_text(unsigned long bits, int prec, int alt, int upper)
{
    const char *hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    int ex = (int) ((bits >> 23) & 0xff), n = 0, i, x, lead = 1, nd;
    unsigned long frac = bits & 0x7fffffUL;

    if (!ex && !frac) {
        lead = 0;
        x = 0;
    } else if (!ex) {
        x = -126;
        while (!(frac & 0x800000UL)) {
            frac <<= 1;
            x--;
        }
        frac &= 0x7fffffUL;
    } else {
        x = ex - 127;
    }
    frac <<= 1;                         /* 24 bits, six hex digits */
    nd = 6;
    if (prec >= 0 && prec < 6) {
        int drop = 4 * (6 - prec);
        unsigned long half = 1UL << (drop - 1), rest = frac & ((1UL << drop) - 1);

        frac >>= drop;
        /* A tie goes to the even one -- and with no digits kept after the
         * point, the digit that is odd or even is the one in front of it. */
        if (rest > half || (rest == half && ((prec ? frac : lead) & 1)))
            frac++;
        if (frac >> (4 * prec)) {
            frac &= (1UL << (4 * prec)) - 1;
            lead++;
        }
        nd = prec;
    } else if (prec < 0) {
        while (nd > 0 && !(frac & 0xf)) {
            frac >>= 4;
            nd--;
        }
    }
    ftext[n++] = '0';
    ftext[n++] = upper ? 'X' : 'x';
    ftext[n++] = hex[lead];
    if (nd > 0 || alt || prec > 0)
        ftext[n++] = '.';
    for (i = nd - 1; i >= 0; i--)
        ftext[n++] = hex[(frac >> (4 * i)) & 0xf];
    for (i = nd; i < prec; i++)
        ftext[n++] = '0';
    n += put_exponent(ftext + n, x, upper ? 'P' : 'p') ;
    /* %a's exponent has as many digits as it needs, and no fewer: 1, not 01. */
    if (ftext[n - 2] == '0' && (x > -10 && x < 10)) {
        ftext[n - 2] = ftext[n - 1];
        n--;
    }

    return n;
}

/* One floating conversion: c is which, the rest what the format said.
 * Answers the text, sign and all, in a buffer of this file's, with its
 * length -- and in *zeros_at where zeros go when the `0` flag pads it: after
 * the sign, after a %a's 0x as well, and nowhere at all for inf and nan.
 * printf lays it out in its field, as it does a number. */
int acc_format_float(int c, int flags, int prec, double value,
                     const char **text, int *zeros_at)
{
    union { float f; unsigned long u; } v;
    unsigned long bits;
    int upper = c == 'F' || c == 'E' || c == 'G' || c == 'A';
    int n, ex, s = 0;

    v.f = (float) value;
    bits = v.u;
    if (bits >> 31)
        fsign[s++] = '-';
    else if (flags & FL_PLUS)
        fsign[s++] = '+';
    else if (flags & FL_SPACE)
        fsign[s++] = ' ';
    bits &= 0x7fffffffUL;
    ex = (int) (bits >> 23);

    if (ex == 0xff) {
        const char *word = bits & 0x7fffffUL ? (upper ? "NAN" : "nan")
                                             : (upper ? "INF" : "inf");

        memcpy(ftext, word, 3);
        n = 3;
        *zeros_at = -1;
    } else {
        if (prec < 0 && c != 'a' && c != 'A')
            prec = 6;
        if (prec > FDIGITS_MAX - 45)
            prec = FDIGITS_MAX - 45;    /* past this the digits are zeros */
        switch (c) {
        case 'f': case 'F': n = fixed_text(bits, prec, flags & FL_ALT); break;
        case 'e': case 'E': n = exp_text(bits, prec, flags & FL_ALT, upper, NULL); break;
        case 'g': case 'G': n = general_text(bits, prec, flags & FL_ALT, upper); break;
        default:            n = hex_text(bits, prec, flags & FL_ALT, upper); break;
        }
        *zeros_at = s + (c == 'a' || c == 'A' ? 2 : 0);
    }

    /* The sign goes in front of the text, which was built with room for it. */
    memmove(ftext + s, ftext, (size_t) n);
    memcpy(ftext, fsign, (size_t) s);
    *text = ftext;

    return n + s;
}
