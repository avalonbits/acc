/*
 * The natural logarithm, and the three written in terms of it.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * x is split into m * 2^k with m between the two square roots of two, so
 * that s = (m-1)/(m+1) is never above 0.172 and the series in s converges
 * quickly: four terms carry it to better than one ulp. The k*ln2 that goes
 * back on comes in two pieces, the larger one exact.
 *
 * log1p has the series in s directly for small x, where forming 1+x first
 * would throw away the digits the answer is made of.
 *
 * Measured against the host's own library: log, log2 within one ulp over
 * the whole range, log10 within two, log1p within three.
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

#define LOG_R0 0.333333343f
#define LOG_R1 0.200000614f
#define LOG_R2 0.142754108f
#define LOG_R3 0.116652295f
#define LN2_HI 0.693115234f
#define LN2_LO 3.19461833e-05f
#define LOG2E 1.44269502f
#define LOG10E 0.434294492f
#define SQRT2 1.41421354f

/* log(x) = k*ln2 + 2s(1 + s^2 R(s^2)), s = (m-1)/(m+1). */
double log(double x)
{
    uint32_t u = bits(x);
    int k;
    double m, s, t, p;

    if ((u & 0x7fffffffu) > 0x7f800000u) return x;
    if ((u & 0x7fffffffu) == 0) return __acc_range_error(-__acc_inf());
    if (u & 0x80000000u)        return __acc_domain_error();
    if (u >= 0x7f800000u)       return x;

    k = (int) (u >> 23);
    if (k == 0) {                       /* subnormal: scale it up first */
        x = x * 16777216.0f;
        u = bits(x);
        k = (int) (u >> 23) - 24;
    }
    k -= 127;
    m = from_bits((u & 0x807fffffu) | ((uint32_t) 127 << 23));
    if (m > SQRT2) { m = m * 0.5f; k++; }

    s = (m - 1.0f) / (m + 1.0f);
    t = s * s;
    p = LOG_R3;
    p = p * t + LOG_R2;
    p = p * t + LOG_R1;
    p = p * t + LOG_R0;

    return (double) k * LN2_HI + ((double) k * LN2_LO + 2.0f * s * (1.0f + t * p));
}

double log2(double x)
{
    return log(x) * LOG2E;
}

double log10(double x)
{
    return log(x) * LOG10E;
}

/* log(1+x) where x is small: the subtraction 1+x would lose is avoided by
 * working the series in x itself. */
double log1p(double x)
{
    if (x == -1.0f)
        return __acc_range_error(-__acc_inf());
    if (x < -1.0f)
        return __acc_domain_error();
    if (x > -0.25f && x < 0.25f) {
        double s = x / (2.0f + x), t = s * s, p = LOG_R3;

        p = p * t + LOG_R2;
        p = p * t + LOG_R1;
        p = p * t + LOG_R0;

        return 2.0f * s * (1.0f + t * p);
    }

    return log(1.0f + x);
}

float logf(float x)   { return log(x); }
float log2f(float x)  { return log2(x); }
float log10f(float x) { return log10(x); }
float log1pf(float x) { return log1p(x); }
