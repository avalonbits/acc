/*
 * The square root, worked out a bit at a time.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Not Newton's method, which is what a square root usually is here: each of
 * its steps is a floating divide, and a divide on this chip is a call into
 * the runtime that costs more than this whole loop. This is the school
 * algorithm instead -- the one for square roots on paper, in base two,
 * where every step is a compare, a subtract and a shift.
 *
 * It is also exact. The loop produces the significand's bits one at a time,
 * largest first, and what is left over at the end says which way to round;
 * there is no error to bound because nothing is approximated. That is worth
 * more than the speed: a square root that is out by an ulp puts the error
 * into everything built on it, and hypot and the rest here are built on it.
 * It was checked against the host's own square root for every one of the
 * 2,139,095,039 positive floats there are, normal and subnormal, and agrees
 * with all of them.
 *
 * The idea is the one from school. To find the root of X, keep a partial
 * root Q and a remainder. At each step the next bit b of the root is worth
 * trying: (Q + b)^2 = Q^2 + 2Qb + b^2, so the amount the remainder must
 * cover is 2Qb + b^2, which in base two with b a single bit is just
 * (2Q + b) * b -- a shift and an add. If the remainder covers it the bit is
 * a one and the remainder comes down; if not it is a zero. `s` below is 2Q
 * carried along so that it never has to be worked out.
 */
#include <math.h>
#include <stdint.h>

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

double sqrt(double x)
{
    uint32_t u = bits(x), m, q, r, s, t;
    int e;

    if ((u & 0x7fffffffu) == 0)
        return x;                       /* both zeros are their own root */
    if ((u & 0x7fffffffu) > 0x7f800000u)
        return x;                       /* not a number */
    if (u & 0x80000000u)
        return __acc_domain_error();    /* every negative, and -infinity */
    if (u >= 0x7f800000u)
        return x;                       /* +infinity */

    /* The significand with its leading bit put back, and the exponent the
     * root will have. A subnormal is shifted up until the leading bit is
     * where a normal number's would be, and its exponent follows. */
    e = (int) (u >> 23);
    m = u & 0x007fffffu;
    if (e == 0) {
        int n = 0;

        while ((m & 0x00800000u) == 0) {
            m = m << 1;
            n++;
        }
        e = 1 - n;
    } else {
        m = m | 0x00800000u;
    }
    e = e - 127;

    /* The root of an odd power of two is not a power of two, so the
     * exponent is made even and the significand takes the difference. */
    if (e & 1) {
        m = m << 1;
        e--;
    }
    e = e >> 1;

    /* One more doubling, so that the 24 bits of root the loop produces come
     * out with a spare bit at the bottom to round with. */
    m = m << 1;

    q = 0;
    s = 0;
    r = 0x01000000u;
    while (r != 0) {
        t = s + r;                      /* (2Q + b), scaled */
        if (t <= m) {
            s = t + r;                  /* 2(Q + b) */
            m = m - t;
            q = q + r;
        }
        m = m << 1;
        r = r >> 1;
    }

    /* What is left of the remainder says the root is not exact, and the
     * bottom bit of q is the one below the significand. Round to nearest,
     * ties to even, which is what every other operation here does. */
    if (m != 0)
        q = q + (q & 1);
    q = q >> 1;

    /* q still carries the leading bit the significand does not store, so
     * it is worth one exponent of its own -- which is why the bias put back
     * here is one less than the 127 that was taken off. */
    return from_bits(q + ((uint32_t) (e + 126) << 23));
}

float sqrtf(float x)
{
    return sqrt(x);
}
