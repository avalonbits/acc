/*
 * The sine, the cosine and the tangent.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * All three are the same reduction and two polynomials. x is written as
 * n quarter turns plus what is left over, which is never more than pi/4,
 * and the quarter says which polynomial answers and with which sign.
 *
 * pi/2 is taken away in five pieces rather than one. Each piece carries
 * only its top bits, so n times it is exact for every n up to 2^16, and
 * what one subtraction loses the next puts back. In one piece the product
 * would round by an ulp of a number as large as x, and all of that lands in
 * a remainder that is small -- which is how a sine of sixty comes out wrong
 * in the third digit. Past about a hundred thousand the pieces run out and
 * the answer degrades, as it does in every library this size.
 *
 * Measured against the host's own library: sine within two ulp over
 * [-100, 100], cosine within eight -- the eight being where the cosine
 * itself passes through zero and has no digits left to be right about.
 *
 * The polynomials were fitted at Chebyshev nodes and their coefficients
 * rounded to float; the fitting is in the repository's history, not here.
 * A double is four bytes on this machine, so these are the double-named
 * functions and the ones with an `f` are the same code.
 */
#include <math.h>
#include <stdint.h>

/* The bits of a float, and a float from bits. Each of these files has its
 * own pair: a library member is taken or left whole, so a shared one would
 * be a member every program taking a sine had to link. */
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

#define SIN_S0 -0.166666672f
#define SIN_S1 0.00833333191f
#define SIN_S2 -0.00019840087f
#define SIN_S3 2.72499256e-06f
#define COS_C0 0.0416666679f
#define COS_C1 -0.00138888881f
#define COS_C2 2.48006036e-05f
#define COS_C3 -2.73011921e-07f
#define TWO_OVER_PI 0.636619747f
#define PIO2_0 1.5703125f
#define PIO2_1 0.000482559204f
#define PIO2_2 1.2665987e-06f
#define PIO2_3 9.89530236e-10f
#define PIO2_4 2.55795385e-12f

/* sin and cos share a reduction: x = n*pi/2 + r, |r| <= pi/4. */
static double sin_poly(double r)
{
    double t = r * r, p = SIN_S3;

    p = p * t + SIN_S2;
    p = p * t + SIN_S1;
    p = p * t + SIN_S0;

    return r + r * t * p;
}

static double cos_poly(double r)
{
    double t = r * r, p = COS_C3;

    p = p * t + COS_C2;
    p = p * t + COS_C1;
    p = p * t + COS_C0;

    return 1.0f - 0.5f * t + t * t * p;
}

/* x = n*pi/2 + r with |r| <= pi/4, and which quarter turn n lands in.
 *
 * pi/2 is subtracted in five pieces rather than one. Each piece carries only
 * its top few bits, so n times it is exact for every n up to 2^16, and what
 * the subtraction loses is only what the next piece puts back. Done in one
 * piece the product n*(pi/2) would round, and the rounding -- an ulp of a
 * number as large as x -- lands entirely in r, which is small: that is how a
 * sine of 60 comes out wrong in the third digit. */
static int reduce(double x, double *r)
{
    int n = (int) (x * TWO_OVER_PI + (x < 0.0f ? -0.5f : 0.5f));
    double y = (double) n, t;

    t = x - y * PIO2_0;
    t = t - y * PIO2_1;
    t = t - y * PIO2_2;
    t = t - y * PIO2_3;
    *r = t - y * PIO2_4;

    return n & 3;
}

double sin(double x)
{
    double r;
    int q;

    if ((bits(x) & 0x7fffffffu) >= 0x7f800000u) return __acc_nan();
    q = reduce(x, &r);
    switch (q) {
    case 0: return sin_poly(r);
    case 1: return cos_poly(r);
    case 2: return -sin_poly(r);
    }
    return -cos_poly(r);
}

double cos(double x)
{
    double r;
    int q;

    if ((bits(x) & 0x7fffffffu) >= 0x7f800000u) return __acc_nan();
    q = reduce(x, &r);
    switch (q) {
    case 0: return cos_poly(r);
    case 1: return -sin_poly(r);
    case 2: return -cos_poly(r);
    }
    return sin_poly(r);
}

double tan(double x)
{
    double r, s, c;
    int q;

    if ((bits(x) & 0x7fffffffu) >= 0x7f800000u) return __acc_nan();
    q = reduce(x, &r);
    s = sin_poly(r);
    c = cos_poly(r);

    return (q & 1) ? -c / s : s / c;
}

float sinf(float x) { return sin(x); }
float cosf(float x) { return cos(x); }
float tanf(float x) { return tan(x); }
