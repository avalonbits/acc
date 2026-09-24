/*
 * x * y + z, rounded once.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * In whole numbers: each operand is a 24-bit integer and a power of two,
 * so the product is a 48-bit integer exactly. The product and z are lined
 * up at bit 61 of a 64-bit number, the smaller shifted down to meet the
 * larger with every bit that falls off kept as one sticky bit at the
 * bottom, and added or subtracted. What is left is rounded to the bits the
 * answer has room for -- 24, or fewer among the subnormals -- to nearest,
 * ties to even, and made a float by scaling, which is exact once the
 * rounding is done.
 *
 * A zero, an infinity or a NaN anywhere is left to the arithmetic: x * y
 * is then exact or not a number, and one rounding of the sum is all that
 * happens.
 */
#include <math.h>
#include <stdint.h>

typedef unsigned long long u64;

/* v, which is finite and not zero, as m * 2^e with the top bit of m at 23:
 * a subnormal is shifted up to meet it. */
static void split(double v, uint32_t *m, int *e)
{
    union { float f; uint32_t u; } b;
    int ef;

    b.f = (float) v;
    ef = (int) ((b.u >> 23) & 0xff);
    *m = b.u & 0x7fffffUL;
    if (ef) {
        *m |= 0x800000UL;
        *e = ef - 150;

        return;
    }
    *e = -149;
    while (!(*m & 0x800000UL)) {
        *m <<= 1;
        --*e;
    }
}

static int top_bit(u64 v)
{
    int n = 0;

    while (v >>= 1)
        n++;

    return n;
}

double fma(double x, double y, double z)
{
    uint32_t mx, my, mz;
    int ex, ey, ez, ea, eb, d, t, keep, shift, sp, sz, sa, sb;
    u64 a, b, m, kept, rest, half;

    if (!isfinite(x) || !isfinite(y) || !isfinite(z) || x == 0.0f
        || y == 0.0f)
        return x * y + z;
    if (z == 0.0f)
        return x * y;

    split(x, &mx, &ex);
    split(y, &my, &ey);
    split(z, &mz, &ez);
    sp = __acc_signbit(x) != __acc_signbit(y);
    sz = __acc_signbit(z);

    /* Both with their top bit at 61. */
    a = (u64) mx * my;
    t = top_bit(a);
    a <<= 61 - t;
    ea = ex + ey - (61 - t);
    b = (u64) mz << 38;
    eb = ez - 38;
    sa = sp;
    sb = sz;
    if (eb > ea) {
        m = a; a = b; b = m;
        d = ea; ea = eb; eb = d;
        d = sa; sa = sb; sb = d;
    }

    d = ea - eb;
    if (d > 63)
        b = 1;
    else if (d > 0)
        b = (b >> d) | ((b & (((u64) 1 << d) - 1)) != 0);

    if (sa == sb) {
        m = a + b;
    } else if (a >= b) {
        m = a - b;
    } else {
        m = b - a;
        sa = sb;
    }
    if (!m)
        return 0.0f;                    /* exactly cancelled: +0 */

    /* Round to the bits there is room for: 24, or fewer when the leading
     * bit is below 2^-126, and none at all below 2^-150. */
    t = top_bit(m);
    keep = t + ea >= -126 ? 24 : 24 - (-126 - (t + ea));
    if (keep < 0)
        return sa ? -0.0f : 0.0f;
    shift = t + 1 - keep;
    if (shift <= 0) {
        kept = m << -shift;
    } else {
        kept = shift > 63 ? 0 : m >> shift;
        rest = shift > 63 ? m : m & (((u64) 1 << shift) - 1);
        half = (u64) 1 << (shift - 1);
        if (rest > half || (rest == half && (kept & 1)))
            kept++;
    }

    x = scalbn((double) (unsigned long) kept, ea + shift);

    return sa ? -x : x;
}

float fmaf(float x, float y, float z) { return fma(x, y, z); }
