/*
 * Whole numbers: the four ways of dropping a fraction.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * None of these is an approximation -- each is the bits of the number
 * with the ones below the binary point taken out, and a decision about
 * the one above them. Nothing here relies on how the runtime rounds:
 * where a tie has to break to even, the two candidates are worked out and
 * the even one chosen, rather than the sum being handed to an adder and
 * hoped over.
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

/* The unbiased exponent: what power of two the number's leading bit is
 * worth. 0x80 for an infinity or a NaN, -127 for a zero or a subnormal. */
static int exponent_of(uint32_t u)
{
    return (int) ((u >> 23) & 0xff) - 127;
}

/* Towards zero: the fraction bits below the binary point thrown away. */
double trunc(double x)
{
    uint32_t u = bits(x);
    int e = exponent_of(u);

    if (e >= 23)
        return x;                       /* whole already, or not a number */
    if (e < 0)
        return from_bits(u & SIGN_BIT); /* less than one: the zero it keeps */

    return from_bits(u & ~(FRAC_MASK >> e));
}

double floor(double x)
{
    double t = trunc(x);

    return t > x ? t - 1.0f : t;
}

double ceil(double x)
{
    double t = trunc(x);

    return t < x ? t + 1.0f : t;
}

/* Away from zero at a half, which is what C99 asks of round -- and not what
 * rint does below. */
double round(double x)
{
    double t = trunc(x), frac = x - t;

    if (frac >= 0.5f)
        return t + 1.0f;
    if (frac <= -0.5f)
        return t - 1.0f;

    return t;
}

/* To nearest, ties to even, which is the rounding every other operation on
 * this machine uses. Written as a decision rather than as the usual trick of
 * adding and subtracting 2^23: that trick is only right if the adder rounds
 * to even, and this is the function that would be finding that out. */
double rint(double x)
{
    double t = trunc(x), frac = fabs(x - t);
    double away = t < 0.0f || (t == 0.0f && __acc_signbit(x)) ? t - 1.0f
                                                              : t + 1.0f;

    if (frac < 0.5f)
        return t;
    if (frac > 0.5f)
        return away;

    /* A tie. The even one of the two is the answer, and `t` is a whole
     * number small enough to ask that of -- a tie needs a fraction, so it
     * cannot have come from anything as large as 2^23. */
    return (long) t % 2 == 0 ? t : away;
}

double nearbyint(double x)
{
    return rint(x);                     /* no rounding mode to differ from */
}

long lrint(double x)
{
    return (long) rint(x);
}

long lround(double x)
{
    return (long) round(x);
}

/* The same functions under the names a program that says `float` reaches
 * for. float and double are one type here, so each of these is a jump. */
float floorf(float x)                  { return floor(x); }
float ceilf(float x)                   { return ceil(x); }
float truncf(float x)                  { return trunc(x); }
float roundf(float x)                  { return round(x); }
float rintf(float x)                   { return rint(x); }
float nearbyintf(float x)              { return nearbyint(x); }
