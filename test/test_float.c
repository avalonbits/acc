/*
 * Floating literals, against the C library's strtof on the host.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 *
 * glibc's strtof rounds correctly, so here it is the reference: every
 * literal below has to come out as the same bits, and end at the same place.
 * The cases that matter are the ones strtod got wrong -- a value on or next to
 * the midpoint between two floats, where a conversion that rounds twice or
 * carries too little precision goes the wrong way -- so most of what is
 * checked is built around midpoints: exactly on one, which has to go to the
 * even neighbour, and one digit either side of it. Then subnormals, the
 * overflow edge, hex, and literals too long to keep every digit of.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acc.h"
#define ACC_CTYPE_TABLE                 /* float.c reads it: see ctype.h */
#include "ctype.h"

static int failures = 0;
static long checks = 0;

void acc_error(const char *fmt, ...)
{
    (void) fmt;
    fprintf(stderr, "  FAIL  acc_error during the test\n");
    exit(2);
}

void acc_error_at(int line, const char *fmt, ...)
{
    (void) line; (void) fmt;
    fprintf(stderr, "  FAIL  acc_error_at during the test\n");
    exit(2);
}

/* One literal: the same bits as strtof, ending at the same place. */
static void check(const char *text)
{
    uint32_t got, want;
    float f;
    char *want_end;
    const char *got_end;

    checks++;
    f = strtof(text, &want_end);
    memcpy(&want, &f, sizeof want);
    got_end = float_literal(text, &got);
    if (got == want && got_end == want_end)
        return;
    if (failures < 20)
        fprintf(stderr, "  FAIL %s\n       got %08lx ending at +%d, want %08lx at +%d\n",
                text, (unsigned long) got, (int) (got_end - text),
                (unsigned long) want, (int) (want_end - text));
    failures++;
}

static uint32_t rng = 12345;

static uint32_t rand32(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;

    return rng;
}

static float from_bits(uint32_t bits)
{
    float f;

    memcpy(&f, &bits, sizeof f);

    return f;
}

/* A finite, positive float, weighted towards the ends of the range. */
static uint32_t random_float_bits(void)
{
    uint32_t bits;

    do {
        bits = rand32() & 0x7fffffff;
        switch (rand32() % 4) {
        case 0: bits &= 0x00ffffff; break;          /* subnormal or smallest normals */
        case 1: bits |= 0x7f000000; break;          /* near the top */
        }
    } while ((bits & 0x7f800000) == 0x7f800000);

    return bits;
}

/* The exact decimal of a double, digits and all, which glibc's printf gives
 * when asked for enough of them; and the same with one more nonzero digit on
 * the end of the mantissa, just above. */
static void check_exact(double d)
{
    char text[400], above[400];
    char *e;

    snprintf(text, sizeof text, "%.160e", d);
    check(text);

    strcpy(above, text);
    e = strchr(above, 'e');
    memmove(e + 1, e, strlen(e) + 1);
    *e = '1';
    check(above);
}


/* ------------------------------------------------------------------ */
/* arithmetic                                                          */

/* float_add and the rest against the host's own float, which on this machine
 * rounds correctly. They exist because the compiler cannot use the host's:
 * the one agondev gives the Agon build answers 0x7fffffff where an overflow
 * should give an infinity, so a constant folded there came out different
 * from the same constant folded on the host. These check that the integer
 * arithmetic which replaced it is the IEEE 754 the host implements.
 *
 * A NaN is compared as a NaN rather than by its bits: which NaN an operation
 * gives is not laid down, and acc's is the quiet one whatever the host makes.
 */
static float as_float(uint32_t bits)
{
    float f;

    memcpy(&f, &bits, sizeof f);

    return f;
}

static uint32_t as_bits(float f)
{
    uint32_t bits;

    memcpy(&bits, &f, sizeof bits);

    return bits;
}

static int is_nan_bits(uint32_t b)
{
    return (b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu) != 0;
}

static int check_op(const char *what, uint32_t a, uint32_t b, uint32_t got,
                    float want_f, int *failures)
{
    uint32_t want = as_bits(want_f);

    checks++;
    if (is_nan_bits(want) ? is_nan_bits(got) : got == want)
        return 0;
    if (*failures < 20)
        fprintf(stderr, "  FAIL %s %08lx %08lx: got %08lx, want %08lx\n",
                what, (unsigned long) a, (unsigned long) b,
                (unsigned long) got, (unsigned long) want);
    (*failures)++;

    return 1;
}

/* The values worth trying on purpose: the ends of the range, the smallest of
 * each kind, and the ones a carry out of the top runs into. */
static const uint32_t corners[] = {
    0x00000000u, 0x80000000u,           /* the zeros */
    0x00000001u, 0x80000001u,           /* the smallest denormals */
    0x007fffffu, 0x00800000u,           /* the largest denormal, and past it */
    0x3f800000u, 0xbf800000u,           /* one */
    0x7f7fffffu, 0xff7fffffu,           /* the largest finite */
    0x7f800000u, 0xff800000u,           /* the infinities */
    0x7fc00000u, 0x7f800001u,           /* NaNs */
    0x4b7fffffu, 0x4b800000u,           /* each side of 2^24 */
    0x00000002u, 0x33800000u, 0x73800000u
};

static int arithmetic(void)
{
    int failures = 0;
    size_t i, j;
    int k;

    for (i = 0; i < sizeof corners / sizeof *corners; i++)
        for (j = 0; j < sizeof corners / sizeof *corners; j++) {
            uint32_t a = corners[i], b = corners[j];

            check_op("add", a, b, float_add(a, b),
                     as_float(a) + as_float(b), &failures);
            check_op("sub", a, b, float_add(a, float_neg(b)),
                     as_float(a) - as_float(b), &failures);
            check_op("mul", a, b, float_mul(a, b),
                     as_float(a) * as_float(b), &failures);
            check_op("div", a, b, float_div(a, b),
                     as_float(a) / as_float(b), &failures);
        }

    /* And a spread of ordinary values, whose exponents are kept close enough
     * together often enough that cancellation is actually reached. */
    for (k = 0; k < 200000; k++) {
        uint32_t a = rand32();
        uint32_t b = rand32();
        float x, y;
        int cmp, want;

        if (k % 2)                      /* the same exponent, give or take */
            b = (b & 0x807fffffu) | (a & 0x7f800000u)
                | ((uint32_t) (rand32() % 3) << 23);

        x = as_float(a);
        y = as_float(b);
        check_op("add", a, b, float_add(a, b), x + y, &failures);
        check_op("sub", a, b, float_add(a, float_neg(b)), x - y, &failures);
        check_op("mul", a, b, float_mul(a, b), x * y, &failures);
        check_op("div", a, b, float_div(a, b), x / y, &failures);

        cmp = float_compare(a, b);
        want = (x != x || y != y) ? 2 : x < y ? -1 : x > y ? 1 : 0;
        checks++;
        if (cmp != want) {
            if (failures < 20)
                fprintf(stderr, "  FAIL cmp %08lx %08lx: got %d, want %d\n",
                        (unsigned long) a, (unsigned long) b, cmp, want);
            failures++;
        }

        /* Truncation, for the values a cast to an integer is defined for. */
        if (!is_nan_bits(a) && (a & 0x7fffffffu) < 0x5f000000u) {
            int64_t whole = float_to_int(a);

            checks++;
            if (whole != (int64_t) x) {
                if (failures < 20)
                    fprintf(stderr, "  FAIL int %08lx: got %lld, want %lld\n",
                            (unsigned long) a, (long long) whole,
                            (long long) x);
                failures++;
            }
        }
    }

    if (failures)
        fprintf(stderr, "  %d arithmetic checks failed\n", failures);
    else
        fprintf(stderr, "  arithmetic against the host's float, 0 failed\n");

    return failures;
}

int main(void)
{
    static const char *const fixed[] = {
        "1.00000011920928955", "0.1", "3.4028235e38", "3.4028236e38",
        "3.40282357e38", "1e39", "1e-45", "1.4e-45", "7e-46", "7.1e-46",
        "7.006492321624085e-46", "1.1754943508222875e-38", "1.17549428e-38",
        "0", "0.0", "000.000e5", ".5", "5.", "1e", "1e+", "1e-x", "1.e3",
        "1.5f", "12.5e-3x", "123456789012345678901234567890e-20",
        "0x1.8p3", "0x1.8", "0x.8p1", "0x1p", "0x1.fffffep127",
        "0x1.ffffffp127", "0x1p-149", "0x1p-150", "0x1.000001p-150",
        "0x1.fffffe8p0", "0x1.fffffe7ffffffffffffffffffffffffffffffp0",
        "16777217", "16777217.0000000000000000000000000000000000001",
        "9999999999999999999999999999999999999999e-50", "1e100000000",
        "1e-100000000", "2.5e-1000",
    };
    int i;

    for (i = 0; i < (int) (sizeof fixed / sizeof fixed[0]); i++)
        check(fixed[i]);

    /* Round trips: a float printed with too few, just enough, and many
     * digits, and in hex. */
    for (i = 0; i < 20000; i++) {
        float f = from_bits(random_float_bits());
        char text[80];

        snprintf(text, sizeof text, "%.6e", f);   check(text);
        snprintf(text, sizeof text, "%.8e", f);   check(text);
        snprintf(text, sizeof text, "%.25e", f);  check(text);
        snprintf(text, sizeof text, "%a", f);     check(text);
    }

    /* Midpoints, and one digit either side: the midpoint of a float and the
     * next is exact as a double, and so is the double just below it. */
    for (i = 0; i < 20000; i++) {
        uint32_t bits = random_float_bits();
        double lo = from_bits(bits), hi = from_bits(bits + 1);
        double mid = (lo + hi) / 2;

        if (isinf(hi))
            hi = ldexp(1.0, 128);           /* past the largest float */
        mid = (lo + hi) / 2;
        check_exact(mid);
        check_exact(nextafter(mid, 0.0));
        {
            char text[80];

            snprintf(text, sizeof text, "%a", mid);
            check(text);
        }
    }

    /* Random digit strings, with the point anywhere and exponents across the
     * whole range and past both ends of it. */
    for (i = 0; i < 20000; i++) {
        char text[200];
        int n = 1 + (int) (rand32() % 60), at = (int) (rand32() % (unsigned) (n + 1)), j, len = 0;

        for (j = 0; j < n; j++) {
            if (j == at)
                text[len++] = '.';
            text[len++] = (char) ('0' + rand32() % 10);
        }
        len += sprintf(text + len, "e%d", (int) (rand32() % 110) - 70);
        check(text);
    }

    /* And the short ones most programs are made of -- a few digits, a point
     * near the end, maybe a small exponent -- which take the shortest way
     * through, in 32 or 64 bits rather than the long arithmetic. */
    for (i = 0; i < 40000; i++) {
        char text[40];
        uint32_t whole = rand32() % 1000000000u;
        int places = (int) (rand32() % 4), len;

        len = sprintf(text, "%u", (unsigned) whole);
        if (places && places < len) {
            memmove(text + len - places + 1, text + len - places, (size_t) places + 1);
            text[len - places] = '.';
        }
        if (rand32() % 2)
            sprintf(text + strlen(text), "e%d", (int) (rand32() % 21) - 10);
        check(text);
    }

    /* Integers to float, as an integer constant initialising a float global
     * is converted: against the host's own conversion, which rounds to
     * nearest. Every width of magnitude, each side of 2^24 where rounding
     * starts, and the ends. */
    for (i = 0; i < 100000; i++) {
        uint32_t mag = rand32() >> (rand32() % 32);
        int negative = (int) (rand32() % 2);
        float want;
        uint32_t want_bits, got;

        if (i < 64)                     /* 2^n - 1 and 2^n, for every n */
            mag = ((uint32_t) 1 << (i % 32)) - (uint32_t) (i / 32);
        want = negative ? -(float) mag : (float) mag;
        got = float_from_int(mag, negative);
        memcpy(&want_bits, &want, sizeof want_bits);
        checks++;
        if (got != want_bits) {
            if (failures < 20)
                fprintf(stderr, "  FAIL int %s%lu: got %08lx, want %08lx\n",
                        negative ? "-" : "", (unsigned long) mag,
                        (unsigned long) got, (unsigned long) want_bits);
            failures++;
        }
    }

    if (failures)
        fprintf(stderr, "  %d of %ld failed\n", failures, checks);
    else
        fprintf(stderr, "  %ld literals, 0 failed\n", checks);

    failures += arithmetic();

    return failures ? 1 : 0;
}
