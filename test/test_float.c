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

    return failures ? 1 : 0;
}
