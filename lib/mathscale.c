/*
 * A number taken apart: its fraction, its whole part, its exponent.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * frexp and ldexp undo each other, and modf splits a number in two.
 * Exact, every one of them, for every value they are given.
 */
#include <math.h>
#include <stdint.h>

/* The bits of a float, and a float from bits. Each of these files has its
 * own copy: a library member is taken or left whole, so a shared one would
 * be a member of its own that every program taking a floor had to link, and
 * the two of them together are shorter than the record of the call would
 * be. */
static uint32_t bits(double x)
{
    union { float f; uint32_t u; } v;

    v.f = (float) x;

    return v.u;
}

static double from_bits(uint32_t u)
{
    union { float f; uint32_t u; } v;

    v.u = u;

    return v.f;
}

#define SIGN_BIT   0x80000000u
#define MAG_MASK   0x7fffffffu
#define FRAC_MASK  0x007fffffu

/* The fraction and the whole number, each with x's own sign -- which is why
 * the fraction is not simply the subtraction: -3.0 leaves -0.0, and the
 * subtraction leaves +0.0. */
double modf(double x, double *ip)
{
    double t = trunc(x);

    *ip = t;
    if (__acc_fpclassify(x) == FP_INFINITE)
        return copysign(0.0f, x);

    return copysign(x - t, x);
}

/* x as a fraction in [1/2, 1) and a power of two. A subnormal is scaled up
 * first, so that the exponent field can be read off in one place. */
double frexp(double x, int *exp)
{
    uint32_t u = bits(x);
    int e = (int) ((u >> 23) & 0xff), extra = 0;

    *exp = 0;
    if (e == 0xff || (u & MAG_MASK) == 0)
        return x;                       /* not a number, or a zero */
    if (e == 0) {
        x = x * 16777216.0f;            /* 2^24, which normalises it */
        u = bits(x);
        e = (int) ((u >> 23) & 0xff);
        extra = -24;
    }
    *exp = e - 126 + extra;

    return from_bits((u & (SIGN_BIT | FRAC_MASK)) | ((uint32_t) 126 << 23));
}

/* x * 2^n. Done by multiplying rather than by adding to the exponent field,
 * so that a result that lands among the subnormals is rounded the way the
 * multiplication rounds it; the power of two is split when it is too large
 * to be a number of its own. */
double scalbn(double x, int n)
{
    if (n > 127) {
        x = x * from_bits((uint32_t) (127 + 127) << 23);
        n -= 127;
        if (n > 127) {
            x = x * from_bits((uint32_t) (127 + 127) << 23);
            n -= 127;
            if (n > 127)
                n = 127;
        }
    } else if (n < -126) {
        /* Down in two steps of no more than 2^-126, the smallest power of
         * two that is a normal number. */
        x = x * from_bits((uint32_t) (127 - 126) << 23);
        n += 126;
        if (n < -126) {
            x = x * from_bits((uint32_t) (127 - 126) << 23);
            n += 126;
            if (n < -126)
                n = -126;
        }
    }

    return x * from_bits((uint32_t) (127 + n) << 23);
}

double ldexp(double x, int exp)
{
    return scalbn(x, exp);
}

/* The same functions under the names a program that says `float` reaches
 * for. float and double are one type here, so each of these is a jump. */
float modff(float x, float *ip)        { return modf(x, ip); }
float frexpf(float x, int *exp)        { return frexp(x, exp); }
float ldexpf(float x, int exp)         { return ldexp(x, exp); }
float scalbnf(float x, int exp)        { return scalbn(x, exp); }
