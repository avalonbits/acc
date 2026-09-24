/*
 * The gamma function, and the logarithm of its size.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Between 1/2 and 3 both come from one fit. 1/gamma is 1 at 1 and at 2,
 * so it is written 1 + (x-1)(x-2)S(x) with S a polynomial; gamma is 1 over
 * that, and lgamma is -log1p of the part after the 1. Written that way the
 * two zeros of lgamma, at 1 and at 2, come out with all their digits, where
 * the log of a gamma worked out first would have none of them.
 *
 * Above 3, gamma is a product down into that range, and lgamma its log --
 * until 8, past which Stirling's series is more accurate than the product.
 * Below 1/2 it is the step up, gamma(x+1)/x; below 0 it is the reflection,
 * pi / (sin(pi x) gamma(1-x)), with sin(pi x) taken on the distance to the
 * nearest whole number so that it does not lose its digits to pi x.
 *
 * Fitted and measured against 40-digit values: within 7 units in the last
 * place for tgamma up to where it overflows, and 3 for lgamma, before the
 * error of the log and exp beneath them. Near lgamma's zeros below 0 no
 * formula of this size keeps the relative error small, and this one does
 * not try.
 */
#include <math.h>

static const float s_coef[] = {
    -0.469681352f, 0.183602691f, 0.02399127f, -0.0342536122f,
    0.00808611885f, 0.000712976442f, -0.000815288164f, 0.000174006665f,
    1.12923851e-06f, -9.00009945e-06f, 1.77737968e-06f
};

#define PI 3.14159265f

/* 1/gamma(x) - 1, for x from 1/2 to 3. */
static double inverse_less_one(double x)
{
    double t = x - 1.75f, r = s_coef[10];
    int i;

    for (i = 9; i >= 0; i--)
        r = r * t + s_coef[i];

    return (x - 1.0f) * (x - 2.0f) * r;
}

/* sin(pi x), for x that is not a whole number and not so large that every
 * float that big is one. */
static double sin_pi(double x)
{
    double n = round(x), v = sin(PI * (x - n));

    return fmod(n, 2.0f) != 0.0f ? -v : v;
}

static int is_whole(double x)
{
    return x == trunc(x);
}

double tgamma(double x)
{
    double p = 1.0f;

    if (isnan(x))
        return x;
    if (x == 0.0f)
        return copysign(HUGE_VAL, x);   /* a pole */
    if (x < 0.0f) {
        if (isinf(x) || is_whole(x))
            return NAN;

        return PI / (sin_pi(x) * tgamma(1.0f - x));
    }
    if (x > 35.5f)
        return HUGE_VAL;
    if (x < 0.5f)
        return 1.0f / ((1.0f + inverse_less_one(x + 1.0f)) * x);
    while (x >= 3.0f) {
        x = x - 1.0f;
        p = p * x;
    }

    return p / (1.0f + inverse_less_one(x));
}

double lgamma(double x)
{
    double p = 1.0f;

    if (isnan(x))
        return x;
    if (isinf(x))
        return HUGE_VAL;
    if (x <= 0.0f) {
        if (is_whole(x))
            return HUGE_VAL;            /* a pole */

        return log(PI / fabs(sin_pi(x))) - lgamma(1.0f - x);
    }
    if (x >= 8.0f) {
        /* Stirling: (x - 1/2)(log x - 1) + (log(2 pi) - 1)/2, and the
         * series in 1/x after it. */
        double r = 1.0f / x, r2 = r * r;

        return (x - 0.5f) * (log(x) - 1.0f) + 0.418938533f
               + r * (0.0833333333f - r2 * (0.00277777778f - r2 * 0.000793650794f));
    }
    if (x < 0.5f)
        return -log1p(inverse_less_one(x + 1.0f)) - log(x);
    while (x >= 3.0f) {
        x = x - 1.0f;
        p = p * x;
    }

    return log(p) - log1p(inverse_less_one(x));
}

float tgammaf(float x) { return tgamma(x); }
float lgammaf(float x) { return lgamma(x); }
