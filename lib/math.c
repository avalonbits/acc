/*
 * What a floating-point number is, and its sign.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The four that every other file here uses, and that a program asking
 * about a number rather than working one out wants on its own: the
 * classification C99 names, the two values the machine cannot spell as
 * constants, and the sign taken off or put on.
 *
 * A double is four bytes here, and so is a long double: the chip has no
 * floating-point unit and the arithmetic is single precision, which is
 * what agondev does too. So each function is written once and the name
 * with the `f` on it is the same function.
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

double __acc_inf(void)
{
    return from_bits(0x7f800000u);
}

double __acc_nan(void)
{
    return from_bits(0x7fc00000u);
}

double nan(const char *tag)
{
    (void) tag;                 /* the payload C99 allows, which is not kept */

    return __acc_nan();
}

int __acc_fpclassify(double x)
{
    uint32_t u = bits(x) & MAG_MASK;

    if (u >= 0x7f800000u)
        return u > 0x7f800000u ? FP_NAN : FP_INFINITE;
    if (u < 0x00800000u)
        return u ? FP_SUBNORMAL : FP_ZERO;

    return FP_NORMAL;
}

int __acc_signbit(double x)
{
    return (int) (bits(x) >> 31);
}

double fabs(double x)
{
    return from_bits(bits(x) & MAG_MASK);
}

double copysign(double x, double y)
{
    return from_bits((bits(x) & MAG_MASK) | (bits(y) & SIGN_BIT));
}

/* The same functions under the names a program that says `float` reaches
 * for. float and double are one type here, so each of these is a jump. */
float fabsf(float x)                   { return fabs(x); }
float copysignf(float x, float y)      { return copysign(x, y); }
