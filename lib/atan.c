/*
 * The inverse of the three, and the angle of a point.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The arc tangent is a polynomial on a small interval that two
 * identities bring every argument down to: a reciprocal takes anything
 * above one below it, and the tangent subtraction formula takes what is
 * left below tan(pi/8).
 *
 * The arc sine has its polynomial on [0, 1/2] and reaches the rest through
 * the half angle, which is also how the arc cosine is worked out near the
 * ends -- pi/2 minus an arc sine that is itself near pi/2 would lose the
 * digits that make up the answer.
 *
 * Measured against the host's own library: arc tangent and arc sine within
 * two ulp, arc cosine within one.
 *
 * The polynomials were fitted at Chebyshev nodes and their coefficients
 * rounded to float; the fitting is in the repository's history, not here.
 * A double is four bytes on this machine, so these are the double-named
 * functions and the ones with an `f` are the same code.
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

#define ATAN_A0 -0.333333343f
#define ATAN_A1 0.199999779f
#define ATAN_A2 -0.142841518f
#define ATAN_A3 0.110713653f
#define ATAN_A4 -0.0862467587f
#define ATAN_A5 0.050481379f
#define ASIN_B0 0.166666672f
#define ASIN_B1 0.074999921f
#define ASIN_B2 0.0446476638f
#define ASIN_B3 0.0302691385f
#define ASIN_B4 0.0236118138f
#define ASIN_B5 0.0105744256f
#define ASIN_B6 0.0309745297f
#define PIO2 1.57079637f
#define PIO4 0.785398185f
#define PI 3.14159274f

/* atan(u) = u(1 + t A(t)) on |u| <= tan(pi/8), reached by two identities. */
static double atan_poly(double u)
{
    double t = u * u, p = ATAN_A5;

    p = p * t + ATAN_A4;
    p = p * t + ATAN_A3;
    p = p * t + ATAN_A2;
    p = p * t + ATAN_A1;
    p = p * t + ATAN_A0;

    return u + u * t * p;
}

double atan(double x)
{
    double a = x < 0.0f ? -x : x, v;
    int inverted = 0, folded = 0;

    if ((bits(x) & 0x7fffffffu) >= 0x7f800000u)
        return (bits(x) & 0x7fffffffu) > 0x7f800000u ? __acc_nan()
                                                     : (x < 0.0f ? -PIO2 : PIO2);
    if (a > 1.0f) { a = 1.0f / a; inverted = 1; }
    if (a > 0.414213568f) { a = (a - 1.0f) / (a + 1.0f); folded = 1; }
    v = atan_poly(a);
    if (folded) v = PIO4 + v;
    if (inverted) v = PIO2 - v;

    return x < 0.0f ? -v : v;
}

static double asin_poly(double t)
{
    double p = ASIN_B6;

    p = p * t + ASIN_B5;
    p = p * t + ASIN_B4;
    p = p * t + ASIN_B3;
    p = p * t + ASIN_B2;
    p = p * t + ASIN_B1;
    p = p * t + ASIN_B0;

    return p;
}

/* asin(a) for 0 <= a <= 0.5, which is where the polynomial is fitted. */
static double asin_small(double a)
{
    double t = a * a;

    return a + a * t * asin_poly(t);
}

double asin(double x)
{
    double a = x < 0.0f ? -x : x, v;

    if (a > 1.0f) return __acc_nan();
    if (a <= 0.5f) {
        v = asin_small(a);
    } else {
        /* asin(a) = pi/2 - 2 asin(sqrt((1-a)/2)), which stays accurate as a
         * approaches one -- where asin itself turns over and a subtraction
         * from pi/2 would lose the digits that matter. */
        double t = (1.0f - a) * 0.5f, r = sqrt(t);

        v = PIO2 - 2.0f * (r + r * t * asin_poly(t));
    }

    return x < 0.0f ? -v : v;
}

double acos(double x)
{
    double a = x < 0.0f ? -x : x, t, r, h;

    if (a > 1.0f) return __acc_nan();
    if (a <= 0.5f) {
        double s = asin_small(a);

        return x < 0.0f ? PIO2 + s : PIO2 - s;
    }

    /* Near either end, through the half-angle, where acos keeps its digits
     * and pi/2 minus an asin close to pi/2 would not. */
    t = (1.0f - a) * 0.5f;
    r = sqrt(t);
    h = 2.0f * (r + r * t * asin_poly(t));

    return x > 0.0f ? h : PI - h;
}

double atan2(double y, double x)
{
    double v;

    if (x == 0.0f && y == 0.0f)
        return bits(x) & 0x80000000u ? (y < 0.0f || (bits(y) & 0x80000000u)
                                         ? -PI : PI)
                                      : (bits(y) & 0x80000000u ? -0.0f : 0.0f);
    if (x == 0.0f)
        return y > 0.0f ? PIO2 : -PIO2;
    v = atan(y / x);
    if (x > 0.0f)
        return v;

    return y >= 0.0f ? v + PI : v - PI;
}

float atanf(float x)           { return atan(x); }
float atan2f(float y, float x) { return atan2(y, x); }
float asinf(float x)           { return asin(x); }
float acosf(float x)           { return acos(x); }
