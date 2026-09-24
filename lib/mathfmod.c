/*
 * What is left over, and which of two numbers.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * fmod is exact: every subtraction it makes is between two numbers of
 * the same exponent or one apart, so nothing is lost off the bottom.
 * fmax and fmin are functions rather than the comparison they look like
 * because C99 asks that a NaN on one side be ignored.
 */
#include <math.h>
#include <stdint.h>

/* The bits of a float, and a float from bits. Each of these files has its
 * own pair. They were copied while a link took a library member whole, when
 * a shared pair would have brought a file with it; a link takes functions
 * now, and they are a few instructions each, so the copies stay. */
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

/* What is left of x after taking whole multiples of y from it, with x's own
 * sign. Each step takes away y scaled by a power of two, biggest first, and
 * every one of those subtractions is exact: the two have the same exponent
 * or one more, so nothing is lost off the bottom. */
double fmod(double x, double y)
{
    double ax = fabs(x), ay = fabs(y), t;
    int ex, ey, i;

    if (__acc_fpclassify(y) == FP_ZERO || __acc_fpclassify(x) == FP_INFINITE
        || __acc_fpclassify(x) == FP_NAN || __acc_fpclassify(y) == FP_NAN)
        return __acc_nan();
    if (__acc_fpclassify(y) == FP_INFINITE || ax < ay)
        return x;

    frexp(ax, &ex);
    frexp(ay, &ey);
    t = scalbn(ay, ex - ey);
    for (i = ex - ey; i >= 0; i--) {
        if (ax >= t)
            ax = ax - t;
        t = t * 0.5f;
    }

    return copysign(ax, x);
}

/* x less the multiple of y nearest to it, the tie going to the even
 * multiple, and in `quo` the last bits of that multiple with the sign of
 * x/y. The same walk as fmod's, which is exact, keeping the bit each step
 * decides; then one more step for the rounding, which is exact too, being
 * a subtraction of two numbers within a factor of two. */
double remquo(double x, double y, int *quo)
{
    double ax = fabs(x), ay = fabs(y), t, r;
    int ex, ey, i, negative = __acc_signbit(x) != __acc_signbit(y);
    unsigned q = 0;

    *quo = 0;
    if (__acc_fpclassify(y) == FP_ZERO || __acc_fpclassify(x) == FP_INFINITE
        || __acc_fpclassify(x) == FP_NAN || __acc_fpclassify(y) == FP_NAN)
        return __acc_nan();
    if (__acc_fpclassify(y) == FP_INFINITE)
        return x;

    if (ax >= ay) {
        frexp(ax, &ex);
        frexp(ay, &ey);
        t = scalbn(ay, ex - ey);
        for (i = ex - ey; i >= 0; i--) {
            q <<= 1;
            if (ax >= t) {
                ax = ax - t;
                q |= 1;
            }
            t = t * 0.5f;
        }
    }

    /* Past half of y, or at half with an odd multiple: the next one up is
     * nearer. Half of y is exact unless y is the smallest there is, where
     * twice ax is instead. */
    if (ay < 2.3509887e-38f ? ax + ax > ay || (ax + ax == ay && (q & 1))
                            : ax > 0.5f * ay || (ax == 0.5f * ay && (q & 1))) {
        ax = ax - ay;
        q++;
    }
    r = __acc_signbit(x) ? -ax : ax;
    *quo = negative ? -(int) (q & 7) : (int) (q & 7);

    return r;
}

double remainder(double x, double y)
{
    int q;

    return remquo(x, y, &q);
}

double fdim(double x, double y)
{
    return x > y ? x - y : 0.0f;
}

/* C99 asks that a NaN on one side be ignored, which is the whole of why
 * these are functions and not the comparison they look like. */
double fmax(double x, double y)
{
    if (__acc_fpclassify(x) == FP_NAN)
        return y;
    if (__acc_fpclassify(y) == FP_NAN)
        return x;

    return x > y ? x : y;
}

double fmin(double x, double y)
{
    if (__acc_fpclassify(x) == FP_NAN)
        return y;
    if (__acc_fpclassify(y) == FP_NAN)
        return x;

    return x < y ? x : y;
}

/* The same functions under the names a program that says `float` reaches
 * for. float and double are one type here, so each of these is a jump. */
float fmodf(float x, float y)          { return fmod(x, y); }
float fdimf(float x, float y)          { return fdim(x, y); }
float fmaxf(float x, float y)          { return fmax(x, y); }
float fminf(float x, float y)          { return fmin(x, y); }
float remainderf(float x, float y)     { return remainder(x, y); }
float remquof(float x, float y, int *q) { return remquo(x, y, q); }
