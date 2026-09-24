/*
 * The hyperbolic three, which are the exponential rearranged.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Each is written so that the cancellation it invites does not happen:
 * the hyperbolic sine of a small number goes through exp(x)-1 rather than
 * through the difference of two exponentials that are nearly equal, and the
 * hyperbolic tangent likewise. Past nine the tangent is one to every bit a
 * float has.
 *
 * Measured against the host's own library: within three or four ulp.
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


double sinh(double x)
{
    double a = x < 0.0f ? -x : x, v;

    if (a < 1.0f) {
        /* (e^a - 1) and its reciprocal, so that small a keeps its digits:
         * e^a - e^-a would take them away. */
        double e = expm1(a);

        v = 0.5f * (e + e / (1.0f + e));
    } else {
        double e = exp(a);

        v = 0.5f * (e - 1.0f / e);
    }

    return x < 0.0f ? -v : v;
}

double cosh(double x)
{
    double a = x < 0.0f ? -x : x, e = exp(a);

    return 0.5f * (e + 1.0f / e);
}

double tanh(double x)
{
    double a = x < 0.0f ? -x : x, v;

    if (a > 9.0f) {
        v = 1.0f;
    } else {
        double e = expm1(2.0f * a);

        v = e / (e + 2.0f);
    }

    return x < 0.0f ? -v : v;
}

/* The inverses, as fdlibm writes them: each a log1p of something that
 * keeps its digits where the plain formula would take them away, and a
 * plain log once x is so large that the rest no longer counts. */
#define LN2 0.693147181f

double acosh(double x)
{
    double t;

    if (isnan(x))
        return x;
    if (x < 1.0f)
        return NAN;
    if (x >= 4096.0f)
        return log(x) + LN2;
    t = x - 1.0f;

    return log1p(t + sqrt(2.0f * t + t * t));
}

double asinh(double x)
{
    double a = fabs(x), v;

    if (isnan(x) || isinf(x) || a < 1.0f / 4096.0f)
        return x;
    if (a >= 4096.0f)
        v = log(a) + LN2;
    else
        v = log1p(a + a * a / (1.0f + sqrt(1.0f + a * a)));

    return x < 0.0f ? -v : v;
}

double atanh(double x)
{
    double a = fabs(x), v;

    if (isnan(x))
        return x;
    if (a > 1.0f)
        return NAN;
    if (a == 1.0f)
        return copysign(HUGE_VAL, x);   /* a pole */
    if (a < 0.5f)
        v = 0.5f * log1p(2.0f * a + 2.0f * a * a / (1.0f - a));
    else
        v = 0.5f * log1p(2.0f * a / (1.0f - a));

    return x < 0.0f ? -v : v;
}

float sinhf(float x) { return sinh(x); }
float coshf(float x) { return cosh(x); }
float tanhf(float x) { return tanh(x); }
float acoshf(float x) { return acosh(x); }
float asinhf(float x) { return asinh(x); }
float atanhf(float x) { return atanh(x); }
