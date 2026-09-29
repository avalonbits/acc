/*
 * The runtime's float arithmetic against IEEE single precision.
 *
 * Built twice: by acc, and run on the Agon, where every operation below is
 * a call into lib/rt; and by the host's compiler, whose float arithmetic is
 * IEEE's, rounded to nearest even. Both walk the same operands from the
 * same generator and hash the same results, so the two outputs agree line
 * for line or a result was wrong. Each line is one block of pairs, so a
 * difference says where to look: build with -DDUMP=n to print block n's
 * results one by one.
 *
 * The operands are chosen to reach the corners: random bits of every class,
 * pairs whose exponents are close (alignment, cancellation), denormals, and
 * the special values against everything. NaNs are compared as one NaN,
 * since IEEE leaves which one comes out to the machine.
 */
#include <stdint.h>
#include <stdio.h>

#ifndef BLOCKS
#define BLOCKS 160
#endif
#define PAIRS 256

static uint32_t state = 0x2545f491;

static uint32_t next(void)
{
    uint32_t x = state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;

    return x;
}

static const uint32_t specials[] = {
    0x00000000, 0x80000000, 0x7f800000, 0xff800000, 0x7fc00000,
    0x00000001, 0x807fffff, 0x00800000, 0x7f7fffff, 0x3f800000,
    0xbf800000, 0x4b7fffff, 0x33800000, 0x3f7fffff, 0x00400000
};

/* One operand, of the class r picks. `near` is another operand's bits,
 * whose exponent a near one shares within a few places. */
static uint32_t operand(uint32_t r, uint32_t near)
{
    uint32_t m = next();

    switch (r & 7) {
    case 0: case 1: case 2:
        return m;
    case 3: case 4: case 5: {
        uint32_t e = (near >> 23) & 0xff;
        uint32_t d = (r >> 3) & 31;

        e = e + d - 15;
        if (e > 254 || e == 0)
            e = 127 + (d & 7);
        return (m & 0x807fffffUL) | (e << 23);
    }
    case 6:
        return specials[(r >> 3) % (sizeof specials / sizeof specials[0])];
    default:
        return m & 0x807fffffUL;          /* a denormal, or zero */
    }
}

static uint32_t bits(float f)
{
    union { float f; uint32_t u; } v;

    v.f = f;
    if ((v.u & 0x7f800000UL) == 0x7f800000UL && (v.u & 0x7fffffUL))
        return 0x7fc00000UL;

    return v.u;
}

static float value(uint32_t u)
{
    union { float f; uint32_t u; } v;

    v.u = u;

    return v.f;
}

static uint32_t mix(uint32_t h, uint32_t v)
{
    return (h ^ v) * 0x01000193UL;
}

int main(void)
{
    int blk, i;

    for (blk = 0; blk < BLOCKS; blk++) {
        uint32_t h = 0x811c9dc5UL;

        for (i = 0; i < PAIRS; i++) {
            uint32_t r = next();
            uint32_t ua = operand(r, next());
            uint32_t ub = operand(r >> 8, ua);
            float a = value(ua), b = value(ub);
            uint32_t c = (a < b) | (a == b) << 1 | (a > b) << 2
                         | (a <= b) << 3 | (a >= b) << 4 | (a != b) << 5
                         | (a < 2.5f) << 6 | (b > -1.0f) << 7
                         | (a == 0.0f) << 8 | (b <= 1e-38f) << 9;
            uint32_t sum = bits(a + b), dif = bits(a - b);
            uint32_t pro = bits(a * b), quo = bits(b == 0 ? a : a / b);
            int32_t n24 = (int32_t) (ua << 8) >> 8;        /* 24 bits */
            int32_t n32 = (int32_t) (ua ^ ub);
            uint32_t conv = bits((float) (int) n24)
                            ^ bits((float) (unsigned) (ub & 0xffffffUL)) << 1
                            ^ bits((float) n32) << 2
                            ^ bits((float) (uint32_t) (ua * 0x9e3779b1UL)) << 3
                            ^ bits((float) ((int64_t) n32 * 1048583 + (int64_t) ub)) << 4;
            uint32_t back = 0;

            /* Truncation where the integer can hold it. */
            if ((ua & 0x7fffffffUL) < 0x4b000000UL)
                back = (uint32_t) (int) a & 0xffffffUL;
            if ((ub & 0x7fffffffUL) < 0x4f000000UL)
                back ^= (uint32_t) (int32_t) b << 3;
            if ((ua & 0x7fffffffUL) < 0x5e000000UL) {
                int64_t w = (int64_t) a;

                back ^= (uint32_t) w ^ (uint32_t) (w >> 32) << 7;
            }

#ifdef DUMP
            if (blk == DUMP)
                printf("%d %08lx %08lx: + %08lx - %08lx * %08lx / %08lx "
                       "cmp %02lx conv %08lx back %08lx\n",
                       i, (unsigned long) ua, (unsigned long) ub,
                       (unsigned long) sum, (unsigned long) dif,
                       (unsigned long) pro, (unsigned long) quo,
                       (unsigned long) c, (unsigned long) conv,
                       (unsigned long) back);
#endif
            h = mix(h, sum);
            h = mix(h, dif);
            h = mix(h, pro);
            h = mix(h, quo);
            h = mix(h, c);
            h = mix(h, conv);
            h = mix(h, back);
        }
        printf("%d %08lx\n", blk, (unsigned long) h);
    }

    return 0;
}
