/*
 * e to the power of x, and the two near relatives of it.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * exp(x) is 2^k times exp(r), where k is x/ln2 rounded and r is what is
 * left -- never more than ln2/2, where a polynomial of six terms is good to
 * eight parts in a thousand million. The ln2 taken away comes in two pieces
 * so that k times it is exact; in one piece the product would round, and
 * the rounding would land entirely in r.
 *
 * exp2 does not go through exp: the whole part of x is an exponent and
 * costs nothing, and only the fraction, never more than a half, goes
 * through the multiplication. Worked out as exp(x*ln2) instead it was out
 * by a hundred and twenty-five ulp at the top of the range.
 *
 * expm1 has a series of its own for small x, where subtracting one from a
 * number close to one would take away the digits that matter.
 *
 * Measured against the host's own library over the whole range: exp and
 * exp2 within one ulp, expm1 within three.
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

#define EXP_P0 0.5f
#define EXP_P1 0.166665778f
#define EXP_P2 0.0416665561f
#define EXP_P3 0.00836317334f
#define EXP_P4 0.00139261759f
#define LN2_HI 0.693115234f
#define LN2_LO 3.19461833e-05f
#define LOG2E 1.44269502f

/* x * 2^n, for n within the range the exponent field can take in one step.
 * Not the scalbn in <math.h>: that one is a member of its own and brings
 * frexp and modf with it, which a program working out an exponential has
 * not asked for. */
static double mk_scalbn(double x, int n)
{
    if (n > 127) {
        x = x * from_bits((uint32_t) 254 << 23);
        n -= 127;
        if (n > 127)
            n = 127;
    } else if (n < -126) {
        x = x * from_bits((uint32_t) 1 << 23);
        n += 126;
        if (n < -126)
            n = -126;
    }

    return x * from_bits((uint32_t) (127 + n) << 23);
}

/* exp(x) = 2^k * exp(r), |r| <= ln2/2. */
double exp(double x)
{
    int k;
    double r, p;

    if (x > 88.7228394f)  return __acc_inf();
    if (x < -103.972084f) return 0.0f;

    k = (int) (x * LOG2E + (x < 0.0f ? -0.5f : 0.5f));
    r = (x - (double) k * LN2_HI) - (double) k * LN2_LO;
    p = EXP_P4;
    p = p * r + EXP_P3;
    p = p * r + EXP_P2;
    p = p * r + EXP_P1;
    p = p * r + EXP_P0;

    return mk_scalbn(1.0f + r + r * r * p, k);
}

/* 2^x. Not exp(x*ln2): that product rounds, and for large x the rounding is
 * worth many ulp of the answer. The whole part of x is an exponent, which
 * costs nothing, and only the fraction -- never more than a half -- goes
 * through the multiplication and the polynomial. */
double exp2(double x)
{
    int k;
    double r;

    if (x > 128.0f)  return __acc_inf();
    if (x < -150.0f) return 0.0f;

    k = (int) (x + (x < 0.0f ? -0.5f : 0.5f));
    r = x - (double) k;

    return mk_scalbn(exp(r * LN2_HI + r * LN2_LO), k);
}

/* exp(x)-1, likewise: for small x the 1 would take the answer's digits. */
double expm1(double x)
{
    if (x > -0.25f && x < 0.25f) {
        double p = EXP_P4;

        p = p * x + EXP_P3;
        p = p * x + EXP_P2;
        p = p * x + EXP_P1;
        p = p * x + EXP_P0;

        return x + x * x * p;
    }

    return exp(x) - 1.0f;
}

float expf(float x)   { return exp(x); }
float exp2f(float x)  { return exp2(x); }
float expm1f(float x) { return expm1(x); }
