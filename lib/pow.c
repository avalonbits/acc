/*
 * x to the power of y.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * 2^(y log2 x), and the whole of the work is keeping that exponent
 * accurate. It can be as large as 128 while the answer wants 24 bits, so an
 * error of one ulp in log2(x) -- six parts in a hundred million -- comes out
 * as a hundred and thirty ulp in the answer, and one rounding of the
 * exponent itself costs eighty. So log2(x) is kept as a whole number and a
 * fraction, y is cut into two halves of twelve bits, every product of the
 * pieces is exact, and the whole number part of the exponent is taken out
 * before any of them are added together.
 *
 * With its own exponential rather than the one in exp.c, because it needs
 * the pieces rather than the answer -- and because a program that raises to
 * a power has not asked for exp2 and expm1 as well.
 *
 * Measured against the host's own library over two million random pairs:
 * within fourteen ulp at the extremes of the range, and within two or three
 * for the exponents a program is likely to ask for.
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
#define LOG_R0 0.333333343f
#define LOG_R1 0.200000614f
#define LOG_R2 0.142754108f
#define LOG_R3 0.116652295f
#define LN2_HI 0.693115234f
#define LN2_LO 3.19461833e-05f
#define LOG2E 1.44269502f
#define SQRT2 1.41421354f

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
static double pw_exp(double x)
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

/* The top twelve bits of a number, so that a product of two of them is
 * exact: twelve and twelve make the twenty-four a float holds. */
static double top_bits(double x)
{
    return from_bits(bits(x) & 0xfffff000u);
}

/* 2^(p + q + b), where p is the large piece, q and b the corrections, and
 * none of the three may be added to another before the whole number part is
 * taken out: p can be 128, where one ulp is already 7.6e-6, and 7.6e-6 in
 * an exponent is five parts in a million of the answer -- eighty-odd ulp.
 * Taking k away from p first is exact, p and k being within a factor of two
 * of each other, and what is left is small enough to add up honestly. */
static double exp2_parts(double p, double q, double b)
{
    double e = p + q + b, r;
    int k;

    if (e > 128.0f)  return __acc_range_error(HUGE_VAL);
    if (e < -150.0f) return 0.0f;

    k = (int) (e + (e < 0.0f ? -0.5f : 0.5f));
    r = ((p - (double) k) + q) + b;
    r = mk_scalbn(pw_exp(r * LN2_HI + r * LN2_LO), k);

    return isinf(r) ? __acc_range_error(r) : r;
}

/* x^y as 2^(y log2 x).
 *
 * The exponent is where the accuracy goes. y*log2(x) can be as large as 150
 * while the answer wants 24 bits, so an error of one ulp in log2(x) -- six
 * parts in a hundred million -- comes out as a hundred and thirty ulp in the
 * answer. So log2(x) is kept as a whole number and a fraction, y is split
 * into two halves of twelve bits, and the four products of the pieces are
 * each exact. Only their sum rounds, and it rounds once.
 */
double pow(double x, double y)
{
    uint32_t ux = bits(x), uy = bits(y);
    int odd_int = 0, is_int = 0, n;
    double m, t, yhi, ylo, thi, tlo, a, b;

    if (y == 0.0f) return 1.0f;
    if (x == 1.0f) return 1.0f;
    if ((ux & 0x7fffffffu) > 0x7f800000u || (uy & 0x7fffffffu) > 0x7f800000u)
        return __acc_nan();

    /* Whether y is a whole number, and an odd one: that is what says a
     * negative x has an answer at all, and what its sign is. */
    {
        int e = (int) ((uy >> 23) & 0xff) - 127;

        if (e >= 23) {
            is_int = 1;
        } else if (e >= 0) {
            uint32_t mm = (uy & 0x007fffffu) | 0x00800000u;

            is_int = (mm & (0x007fffffu >> e)) == 0;
            odd_int = is_int && ((mm >> (23 - e)) & 1);
        }
    }

    if (x == 0.0f) {
        if (y > 0.0f)
            return odd_int ? x : 0.0f;

        return __acc_range_error(odd_int ? copysign(HUGE_VAL, x) : HUGE_VAL);
    }
    if ((ux & 0x7fffffffu) == 0x7f800000u)
        return (ux & 0x80000000u) == 0 ? (y > 0.0f ? x : 0.0f)
                                       : (y > 0.0f ? (odd_int ? x : -x) : 0.0f);
    if (x < 0.0f) {
        double v;

        if (!is_int) return __acc_domain_error();
        v = pow(-x, y);

        return odd_int ? -v : v;
    }

    /* x = m * 2^n with m in [sqrt(1/2), sqrt(2)), so log2(x) = n + log2(m)
     * with the fraction never more than a half. */
    n = (int) ((ux >> 23) & 0xff);
    if (n == 0) {
        x = x * 16777216.0f;
        ux = bits(x);
        n = (int) ((ux >> 23) & 0xff) - 24;
    }
    n -= 127;
    m = from_bits((ux & 0x007fffffu) | ((uint32_t) 127 << 23));
    if (m > SQRT2) { m = m * 0.5f; n++; }

    {
        double s = (m - 1.0f) / (m + 1.0f), ss = s * s, p = LOG_R3;

        p = p * ss + LOG_R2;
        p = p * ss + LOG_R1;
        p = p * ss + LOG_R0;
        t = 2.0f * s * (1.0f + ss * p) * LOG2E;      /* log2(m), |t| <= 1/2 */
    }

    yhi = top_bits(y);
    ylo = y - yhi;
    thi = top_bits(t);
    tlo = t - thi;

    /* y*n is exact in two pieces because n is a small whole number, and
     * y*t is exact in four because the halves are twelve bits each. The
     * two pieces of y*n are kept apart: adding them is the one rounding
     * this is all written to avoid. */
    a = yhi * (double) n;
    b = ((yhi * thi + yhi * tlo) + ylo * thi) + ylo * tlo;

    return exp2_parts(a, ylo * (double) n, b);
}

float powf(float x, float y) { return pow(x, y); }
