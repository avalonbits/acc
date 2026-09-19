/*
 * Floating literals: the text of one, to the nearest IEEE 754 single.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This was strtod, and it was wrong in two ways. On the Agon a double is
 * the same 32 bits as a float and agondev's strtod does not round correctly:
 * `1.00000011920928955`, which is 1 + 2^-23 exactly, came out as 1 + 3*2^-23.
 * And on the host, where strtod is exact, the double it returned was rounded
 * a second time on the way to a float, which is wrong in its own rarer cases
 * -- a literal just past the midpoint between two floats can land on it as a
 * double and then go the wrong way. So the two builds of acc disagreed about
 * some literals, and neither was always right.
 *
 * What is done instead is exact, and only integers: the literal is a fraction
 * num / den times a power of two, both big integers; it is scaled so that
 * the integer part of the quotient has 25 or 26 bits, and that is rounded
 * once, to nearest with ties to even, at whichever bit the result's exponent
 * leaves for the last -- the 24th for a normal number, fewer for a subnormal.
 * The remainder of the division says whether anything lies past the bits
 * kept, which is all rounding needs to know about the rest.
 */

#include <stdint.h>
#include <string.h>

#include "acc.h"
#include "ctype.h"

/* Significant digits kept. A literal can have any number, but the boundary
 * between two floats -- the only thing the rounding compares against -- never
 * has more than 113 significant decimal digits: it is an odd number below
 * 2^25 times a power of two no smaller than 2^-150, and 5^150 has 105. So
 * digits past the 120th can only say whether the value is above what the
 * first 120 make, which is one bit, and they are kept as that bit. A hex
 * literal is the same with 32 digits, 128 bits against the 26 a boundary
 * has. */
#define DIGITS      120
#define HEX_DIGITS  32

/* And for a double, whose boundaries are longer: 767 digits is the bound a
 * double's own boundary reaches, which no arithmetic here could hold -- the
 * numerator would be a kilobyte. 340 is what is kept, and the digits past it
 * say only that there is more, which is the sticky bit. So a long double
 * literal is correctly rounded unless its first 340 significant digits land
 * exactly on the boundary between two doubles and the ones after decide it,
 * which no literal anybody writes does. */
#define DIGITS_D    340

/* A non-negative integer, a byte to a limb, lowest first. Bytes because a
 * byte times a byte fits in the 24 bits an int has on the Agon, so every step
 * below is native arithmetic there.
 *
 * The largest one is the denominator of the smallest literal that is not
 * simply zero -- 120 digits past the 46th place, 10^166, is 552 bits -- and
 * then the 26 the division shifts it by. Static, because the frames on the
 * Agon have to stay small, and there are three. */
#define LIMBS 300

typedef struct {
    unsigned char b[LIMBS];
    int           n;            /* bytes in use; the top one is not zero */
} Big;

static Big num, den, part;

/* The sizes above are worked out, not measured, so a mistake in them is an
 * internal error rather than a write past the end. */
static void big_room(const Big *a, int more)
{
    if (a->n + more > LIMBS)
        acc_error("internal: a floating literal needs more than %d bytes", LIMBS);
}

static void big_set(Big *a, unsigned v)
{
    memset(a->b, 0, sizeof a->b);
    a->n = 0;
    while (v) {
        a->b[a->n++] = (unsigned char) v;
        v >>= 8;
    }
}

/* a = a * m + add, for m and add below 256. */
static void big_mul_add(Big *a, unsigned m, unsigned add)
{
    unsigned carry = add;
    int i;

    for (i = 0; i < a->n; i++) {
        unsigned t = a->b[i] * m + carry;

        a->b[i] = (unsigned char) t;
        carry = t >> 8;
    }
    if (carry) {
        big_room(a, 1);
        a->b[a->n++] = (unsigned char) carry;
    }
}

static int big_bits(const Big *a)
{
    int bits;
    unsigned top;

    if (a->n == 0)
        return 0;
    bits = (a->n - 1) * 8;
    for (top = a->b[a->n - 1]; top; top >>= 1)
        bits++;

    return bits;
}

static void big_shl(Big *a, int bits)
{
    int bytes = bits / 8, i;

    bits %= 8;
    if (a->n == 0)
        return;
    big_room(a, bytes + 1);
    for (i = a->n - 1 + bytes + 1; i >= 0; i--) {
        unsigned hi = (i - bytes < a->n && i - bytes >= 0) ? a->b[i - bytes] : 0;
        unsigned lo = (i - bytes - 1 < a->n && i - bytes - 1 >= 0)
                      ? a->b[i - bytes - 1] : 0;

        a->b[i] = (unsigned char) ((hi << bits) | (lo >> (8 - bits)));
    }
    a->n += bytes + 1;
    while (a->n && a->b[a->n - 1] == 0)
        a->n--;
}

static void big_shr1(Big *a)
{
    int i;

    for (i = 0; i < a->n; i++)
        a->b[i] = (unsigned char) ((a->b[i] >> 1)
                                   | (i + 1 < a->n ? (a->b[i + 1] & 1) << 7 : 0));
    while (a->n && a->b[a->n - 1] == 0)
        a->n--;
}

static int big_cmp(const Big *a, const Big *b)
{
    int i;

    if (a->n != b->n)
        return a->n < b->n ? -1 : 1;
    for (i = a->n - 1; i >= 0; i--)
        if (a->b[i] != b->b[i])
            return a->b[i] < b->b[i] ? -1 : 1;

    return 0;
}

/* a -= b, where a >= b. */
static void big_sub(Big *a, const Big *b)
{
    int borrow = 0, i;

    for (i = 0; i < a->n; i++) {
        int t = a->b[i] - (i < b->n ? b->b[i] : 0) - borrow;

        borrow = t < 0;
        a->b[i] = (unsigned char) (t + (borrow ? 256 : 0));
    }
    while (a->n && a->b[a->n - 1] == 0)
        a->n--;
}

/* A Big of at most eight bytes as a 64-bit integer. */
static uint64_t big_to_u64(const Big *a)
{
    uint64_t v = 0;
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    int i;

    for (i = a->n - 1; i >= 0; i--)
        v = (v << 8) | a->b[i];
#else
    memcpy(&v, a->b, (size_t) a->n);
#endif

    return v;
}

/* How many bits v has, found a byte at a time: a 64-bit shift is a call into
 * the runtime on the Agon, and an 8-bit one is an instruction. */
static int bits64(uint64_t v)
{
    unsigned char b[8];
    int n = 8, bits;
    unsigned top;

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (n = 0; n < 8; n++)
        b[n] = (unsigned char) (v >> (8 * n));
    n = 8;
#else
    memcpy(b, &v, sizeof b);
#endif
    while (n && b[n - 1] == 0)
        n--;
    if (n == 0)
        return 0;
    bits = (n - 1) * 8;
    for (top = b[n - 1]; top; top >>= 1)
        bits++;

    return bits;
}

static int hex_value(int c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

/* An exponent's digits, held to a bound far past any that could matter so
 * that a long one cannot overflow the int it is read into. */
static const char *read_exponent(const char *p, long *out)
{
    int negative = 0;
    long e = 0;

    if (*p == '+' || *p == '-')
        negative = (*p++ == '-');
    while (is_digit((unsigned char) *p)) {
        if (e < 100000)
            e = e * 10 + (*p - '0');
        p++;
    }
    *out = negative ? -e : e;

    return p;
}

/* The float nearest q * 2^(e - bl + 1), where q has bl = 25 or 26 bits and
 * so e is the exponent of its leading one, and sticky says there is a little
 * more than q: rounded once, to nearest with ties to even, at the bit the
 * exponent leaves for the last -- the 24th for a normal number, fewer for a
 * subnormal. */
static uint32_t float_pack(uint32_t q, int e, int sticky)
{
    int bl = (q >> 25) ? 26 : 25;
    int p_bits = (e >= -126) ? 24 : e + 150;    /* bits the result can hold */
    int drop;
    uint32_t half, rem, m;

    if (p_bits < 0)
        return 0;                               /* under half the smallest */

    drop = bl - p_bits;
    half = (uint32_t) 1 << (drop - 1);
    rem = q & ((half << 1) - 1);
    m = q >> drop;
    if (rem > half || (rem == half && (sticky || (m & 1))))
        m++;

    /* Subnormal, and a mantissa that rounded up to 2^23 is the smallest
     * normal number, which is what those bits say without help. */
    if (e < -126)
        return m;
    if (m == (uint32_t) 1 << 24) {
        m >>= 1;
        e++;
    }
    if (e > 127)
        return 0x7f800000;

    return ((uint32_t) (e + 127) << 23) | (m & 0x7fffff);
}

/* The float nearest an integer, given as a magnitude and a sign: what a
 * global's initial value is when an integer constant initialises a float.
 * Exact for anything under 2^24, and rounded like a literal above it. */
uint32_t float_from_int(uint32_t magnitude, int negative)
{
    int bl = bits64(magnitude), sticky = 0;
    uint32_t q = magnitude;

    if (magnitude == 0)
        return negative ? 0x80000000 : 0;
    if (bl < 26) {
        q <<= 26 - bl;
    } else {
        sticky = (q & (((uint32_t) 1 << (bl - 26)) - 1)) != 0;
        q >>= bl - 26;
    }

    return float_pack(q, bl - 1, sticky) | (negative ? 0x80000000 : 0);
}

/* The literal at s: its bits as a float, and where it ends. It ends at s if
 * there are no digits. The grammar is strtod's for a literal with no sign:
 * decimal digits with a point and an exponent, each optional, or 0x and hex
 * digits with a point and a binary exponent -- an exponent is only taken if a
 * digit follows the letter, and the point needs no digits either side of it
 * so long as there is one somewhere. */
/* What the digits of a literal came to: `num` holds the ones kept, and this
 * says how they are to be read. Shared by the float and the double, which
 * differ only in how many digits they keep and in what is done with them
 * afterwards. */
typedef struct {
    int  hex;           /* 0x, with a binary exponent */
    int  kept;          /* digits in num; 0 means the value is zero */
    int  seen;          /* any digits at all: no digits is not a literal */
    int  sticky;        /* a digit past the last kept one was not zero */
    long scale;         /* a power of the base the kept digits are off by */
    long power2;        /* and a power of two on top, for hex */
} Digits;

/* The literal's digits, into num and g; where the literal ends is returned.
 * `most` is how many digits are kept -- past that they only make the value
 * sticky, since what they can still decide is one bit. */
static const char *literal_digits(const char *s, int most, Digits *g)
{
    const char *p = s;
    int base, point = 0;
    long exp;

    g->hex = (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')
              && (hex_value((unsigned char) p[2]) >= 0
                  || (p[2] == '.' && hex_value((unsigned char) p[3]) >= 0)));
    g->kept = g->seen = g->sticky = 0;
    g->scale = g->power2 = 0;
    base = g->hex ? 16 : 10;

    big_set(&num, 0);
    if (g->hex)
        p += 2;

    for (;; p++) {
        int d;

        if (*p == '.' && !point) {
            point = 1;
            continue;
        }
        d = g->hex ? hex_value((unsigned char) *p)
                   : (is_digit((unsigned char) *p) ? *p - '0' : -1);
        if (d < 0)
            break;
        g->seen = 1;
        if (g->kept == 0 && d == 0) {       /* a leading zero */
            if (point)
                g->scale--;
            continue;
        }
        if (g->kept < most) {
            big_mul_add(&num, (unsigned) base, (unsigned) d);
            g->kept++;
            if (point)
                g->scale--;
        } else {
            if (d)
                g->sticky = 1;
            if (!point)
                g->scale++;
        }
    }
    if (!g->seen)
        return s;

    if (g->hex) {
        if ((*p == 'p' || *p == 'P')
            && (is_digit((unsigned char) p[1])
                || ((p[1] == '+' || p[1] == '-') && is_digit((unsigned char) p[2])))) {
            p = read_exponent(p + 1, &exp);
            g->power2 = exp;
        }
        g->power2 += g->scale * 4;
        g->scale = 0;
    } else if ((*p == 'e' || *p == 'E')
               && (is_digit((unsigned char) p[1])
                   || ((p[1] == '+' || p[1] == '-') && is_digit((unsigned char) p[2])))) {
        p = read_exponent(p + 1, &exp);
        g->scale += exp;
    }

    return p;
}

const char *float_literal(const char *s, uint32_t *bits)
{
    Digits g;
    const char *p = literal_digits(s, (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
                                      ? HEX_DIGITS : DIGITS, &g);
    int hex, kept, sticky;
    long scale, power2;
    int num_bits, k, i;
    uint32_t q;

    *bits = 0;
    if (!g.seen)
        return s;
    hex = g.hex;
    kept = g.kept;
    sticky = g.sticky;
    scale = g.scale;
    power2 = g.power2;

    if (kept == 0)
        return p;                           /* zero, however it was written */

    /* Out of range either way, told from where the leading digit falls:
     * 10^39 is past the largest float, and a value below 10^-46 is less than
     * half the smallest one. Before any of the big arithmetic, which would
     * otherwise have to be as big as the exponent. */
    if (!hex) {
        long lead = kept + scale;           /* value is below 10^lead */

        if (lead > 39) {
            *bits = 0x7f800000;
            return p;
        }
        if (lead < -46)
            return p;
    } else {
        long lead = big_bits(&num) + power2;    /* value is below 2^lead */

        if (lead > 129) {
            *bits = 0x7f800000;
            return p;
        }
        if (lead < -150)
            return p;
    }

    /* num / den is in [2^(nb - db - 1), 2^(nb - db + 1)), so scaling it by
     * 2^k puts the quotient in [2^24, 2^26): the 24 bits a float keeps, at
     * least one more to say which side of the halfway point it is, and the
     * remainder to say whether it is exactly on it.
     *
     * Most literals are a few digits and a small exponent, and for those the
     * whole of it fits in an integer the machine divides: one division, where
     * the loop further down is twenty-six rounds of compare, subtract and
     * shift across byte arrays -- some 35,000 cycles a literal on the Agon,
     * which made a benchmark with two dozen of them 7% slower than strtod had
     * been. In 32 bits when the divisor is below 2^7, which is up to two
     * decimal places and covers `1.5` and `0.25` and nearly everything else;
     * in 64 when it is below 2^39, which is eleven, since a 64-bit division
     * is itself a long call there. */
    if (kept + (scale > 0 ? scale : 0) <= (hex ? 8 : 9) && scale >= -2) {
        uint32_t v = (uint32_t) big_to_u64(&num), d = 1;

        for (; scale > 0; scale--)
            v *= 10;
        for (; scale < 0; scale++)
            d *= 10;
        k = 25 - (bits64(v) - bits64(d));
        if (k > 0)
            v <<= k;
        else if (k < 0)
            d <<= -k;
        q = v / d;
        if (v - q * d)
            sticky = 1;
    } else if (kept <= (hex ? 16 : 19) && kept + scale <= 19 && scale >= -11) {
        uint64_t v = big_to_u64(&num), d = 1;

        for (; scale > 0; scale--)
            v *= 10;
        for (; scale < 0; scale++)
            d *= 10;
        k = 25 - (bits64(v) - bits64(d));
        if (k > 0)
            v <<= k;
        else if (k < 0)
            d <<= -k;
        q = (uint32_t) (v / d);
        if (v - q * d)
            sticky = 1;
    } else {
        big_set(&den, 1);
        for (; scale > 0; scale--)
            big_mul_add(&num, 10, 0);
        for (; scale < 0; scale++)
            big_mul_add(&den, 10, 0);

        num_bits = big_bits(&num);
        k = 25 - (num_bits - big_bits(&den));
        if (k > 0)
            big_shl(&num, k);
        else if (k < 0)
            big_shl(&den, -k);

        part = den;
        big_shl(&part, 25);
        q = 0;
        for (i = 25; i >= 0; i--) {
            if (big_cmp(&num, &part) >= 0) {
                big_sub(&num, &part);
                q |= (uint32_t) 1 << i;
            }
            big_shr1(&part);
        }
        if (num.n)
            sticky = 1;
    }

    /* The value is q * 2^(power2 - k), and a little more if sticky. */
    *bits = float_pack(q, ((q >> 25) ? 25 : 24) - k + (int) power2, sticky);

    return p;
}

/* ------------------------------------------------------------------ */
/* double                                                              */

/* A long double is an eight-byte IEEE 754 double, and a literal with an L
 * suffix is one. The same rounding as a float's, at 53 bits of significand
 * and 11 of exponent -- and only the big-integer path, because a long double
 * literal is rare enough that a fast one would be code nothing runs. */
static uint64_t double_pack(uint64_t q, int e, int sticky)
{
    int bl = (q >> 54) ? 55 : 54;
    int p_bits = (e >= -1022) ? 53 : e + 1075;  /* bits the result can hold */
    int drop;
    uint64_t half, rem, m;

    if (p_bits < 0)
        return 0;                               /* under half the smallest */

    drop = bl - p_bits;
    half = (uint64_t) 1 << (drop - 1);
    rem = q & ((half << 1) - 1);
    m = q >> drop;
    if (rem > half || (rem == half && (sticky || (m & 1))))
        m++;

    /* Subnormal, and a mantissa that rounded up to 2^52 is the smallest
     * normal number, which is what those bits say without help. */
    if (e < -1022)
        return m;
    if (m == (uint64_t) 1 << 53) {
        m >>= 1;
        e++;
    }
    if (e > 1023)
        return 0x7ff0000000000000ULL;

    return ((uint64_t) (e + 1023) << 52) | (m & 0xfffffffffffffULL);
}

/* The double nearest an integer, given as a magnitude and a sign: what a
 * global's initial value is when an integer constant initialises a long
 * double. Every value under 2^53 is exact. */
uint64_t double_from_int(uint64_t magnitude, int negative)
{
    int bl = bits64(magnitude), sticky = 0;
    uint64_t q = magnitude, sign = negative ? (uint64_t) 1 << 63 : 0;

    if (magnitude == 0)
        return sign;
    if (bl < 55) {
        q <<= 55 - bl;
    } else {
        sticky = (q & (((uint64_t) 1 << (bl - 55)) - 1)) != 0;
        q >>= bl - 55;
    }

    return double_pack(q, bl - 1, sticky) | sign;
}

/* The literal at s: its bits as a double, and where it ends, as
 * float_literal does it for a float. */
const char *double_literal(const char *s, uint64_t *bits)
{
    Digits g;
    const char *p = literal_digits(s, (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
                                      ? HEX_DIGITS : DIGITS_D, &g);
    int k, i;
    uint64_t q;

    *bits = 0;
    if (!g.seen)
        return s;
    if (g.kept == 0)
        return p;                           /* zero, however it was written */

    /* Out of range either way, told from where the leading digit falls, and
     * before any of the big arithmetic: 10^310 is past the largest double,
     * and a value below 10^-324 is less than half the smallest one. */
    if (!g.hex) {
        long lead = g.kept + g.scale;       /* the value is below 10^lead */

        if (lead > 310) {
            *bits = 0x7ff0000000000000ULL;

            return p;
        }
        if (lead < -324)
            return p;
    } else {
        long lead = big_bits(&num) + g.power2;  /* the value is below 2^lead */

        if (lead > 1025) {
            *bits = 0x7ff0000000000000ULL;

            return p;
        }
        if (lead < -1080)
            return p;
    }

    /* num / den scaled by 2^k, so that the quotient has 54 or 55 bits: the
     * 53 a double keeps, one more to say which side of the halfway point it
     * is on, and the remainder to say whether it is exactly on it. */
    big_set(&den, 1);
    for (; g.scale > 0; g.scale--)
        big_mul_add(&num, 10, 0);
    for (; g.scale < 0; g.scale++)
        big_mul_add(&den, 10, 0);

    k = 54 - (big_bits(&num) - big_bits(&den));
    if (k > 0)
        big_shl(&num, k);
    else if (k < 0)
        big_shl(&den, -k);

    part = den;
    big_shl(&part, 54);
    q = 0;
    for (i = 54; i >= 0; i--) {
        if (big_cmp(&num, &part) >= 0) {
            big_sub(&num, &part);
            q |= (uint64_t) 1 << i;
        }
        big_shr1(&part);
    }
    if (num.n)
        g.sticky = 1;

    /* The value is q * 2^(power2 - k), and a little more if sticky. */
    *bits = double_pack(q, ((q >> 54) ? 54 : 53) - k + (int) g.power2, g.sticky);

    return p;
}
