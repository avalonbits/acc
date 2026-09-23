/*
 * The length of the third side.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Scaled by the larger of the two before the squares are taken, so that
 * a pair a program could hold does not overflow on the way to an answer it
 * could hold too. The square root under it is the exact one, so this is
 * within one ulp.
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

double hypot(double x, double y)
{
    double a = x < 0.0f ? -x : x, b = y < 0.0f ? -y : y, t;
    int e;

    if (a < b) { t = a; a = b; b = t; }
    if (b == 0.0f) return a;
    if ((bits(a) & 0x7fffffffu) >= 0x7f800000u) return a;

    /* Scaled so that neither the squares nor their sum leave the range. */
    {
        uint32_t u = bits(a);

        e = (int) ((u >> 23) & 0xff) - 127;
        a = mk_scalbn(a, -e);
        b = mk_scalbn(b, -e);
    }

    return mk_scalbn(sqrt(a * a + b * b), e);
}

float hypotf(float x, float y) { return hypot(x, y); }
