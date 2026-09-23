/*
 * The cube root.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * A first guess from a third of the exponent and a straight line through
 * the significand, then Newton three times -- each step trebles the digits
 * that are right, and three are more than enough from the eight the guess
 * begins with. Within one ulp over the whole range.
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

double cbrt(double x)
{
    uint32_t u = bits(x) & 0x7fffffffu;
    double a, t;
    int e;

    if (u == 0 || u >= 0x7f800000u) return x;
    a = from_bits(u);

    /* A first guess from a third of the exponent, then Newton twice: each
     * step trebles the digits, and two are enough from eight. */
    e = (int) ((u >> 23) & 0xff) - 127;
    {
        int third = e / 3, rem = e - third * 3;

        if (rem < 0) { third--; rem += 3; }
        t = mk_scalbn(from_bits((u & 0x007fffffu) | ((uint32_t) 127 << 23)), 0);
        t = (0.5874009f + 0.4125991f * t) * (rem == 0 ? 1.0f
                                             : rem == 1 ? 1.2599211f
                                                        : 1.5874011f);
        t = mk_scalbn(t, third);
    }
    t = t - (t - a / (t * t)) * 0.333333343f;
    t = t - (t - a / (t * t)) * 0.333333343f;
    t = t - (t - a / (t * t)) * 0.333333343f;

    return bits(x) & 0x80000000u ? -t : t;
}

float cbrtf(float x) { return cbrt(x); }
