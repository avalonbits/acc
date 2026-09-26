/* Four-byte integers with a constant on the right of & | ^ << >>, which
 * acc does on the left's bytes where that is shorter than calling: masks
 * that keep, clear, set and flip whole bytes and parts of them, shifts by
 * one and by whole bytes each way on signed and unsigned values, results
 * used again, and the same where so many temporaries are alive that the
 * scratch is past a displacement's reach, where the call does it. */
static unsigned long h = 1;

/* The high bits folded down each time: an answer wrong only in its top
 * bit, by a signed shift made unsigned, came out right in pairs with h * 31
 * alone. */
static void mix(unsigned long v)
{
    h = ((h ^ (h >> 13)) * 31 + v) & 0xffffffffUL;
}

static void ops(unsigned long u, long s)
{
    mix(u & 0xffUL); mix(u & 1UL); mix(u & 0xff00ff00UL); mix(u & 0x7fffffffUL);
    mix(u & 0x0f0f0000UL);
    mix(u | 0x80UL); mix(u | 0xff000000UL); mix(u | 0x00ff00ffUL); mix(u | 0x10UL);
    mix(u ^ 0xffUL); mix(u ^ 0xffffffffUL); mix(u ^ 0x80000001UL); mix(u ^ 0x5500UL);
    mix(u >> 1); mix(u >> 8); mix(u >> 16); mix(u >> 24);
    mix(u << 1); mix(u << 8); mix(u << 16); mix(u << 24);
    mix((unsigned long) (s >> 1)); mix((unsigned long) (s >> 8));
    mix((unsigned long) (s >> 16)); mix((unsigned long) (s >> 24));
    mix((unsigned long) (s & 0xffL)); mix((unsigned long) (s | 0x100L));
    mix((u >> 8 & 0xffUL) ^ (u << 24 | 1UL));
    mix(((u ^ 0xffUL) >> 1) & 0x7fUL);
}

static void deep(unsigned long u)
{
    unsigned long x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31;

    x0 = u;
    x1 = x0 + 1UL;
    x2 = x1 + 2UL;
    x3 = x2 + 3UL;
    x4 = x3 + 4UL;
    x5 = x4 + 5UL;
    x6 = x5 + 6UL;
    x7 = x6 + 7UL;
    x8 = x7 + 8UL;
    x9 = x8 + 9UL;
    x10 = x9 + 10UL;
    x11 = x10 + 11UL;
    x12 = x11 + 12UL;
    x13 = x12 + 13UL;
    x14 = x13 + 14UL;
    x15 = x14 + 15UL;
    x16 = x15 + 16UL;
    x17 = x16 + 17UL;
    x18 = x17 + 18UL;
    x19 = x18 + 19UL;
    x20 = x19 + 20UL;
    x21 = x20 + 21UL;
    x22 = x21 + 22UL;
    x23 = x22 + 23UL;
    x24 = x23 + 24UL;
    x25 = x24 + 25UL;
    x26 = x25 + 26UL;
    x27 = x26 + 27UL;
    x28 = x27 + 28UL;
    x29 = x28 + 29UL;
    x30 = x29 + 30UL;
    x31 = x30 + 31UL;
    mix(x31 >> 8); mix(x31 & 0xffUL); mix(x31 << 1); mix(x30 >> 24 | 3UL);
    mix(x0);

    /* Temporaries alive together, each the left of the next: the last
     * shifts and masks are in scratch past a displacement's reach. */
    mix((x1 ^ x2) + ((x3 ^ x4) + ((x5 ^ x6) + ((x7 ^ x8) + ((x9 ^ x10)
        + ((x11 ^ x12) + ((x13 ^ x14) + ((x15 ^ x16) + ((x17 ^ x18)
        + ((x19 >> 8) + ((x20 & 0xffUL) + (x21 << 1))))))))))));
}

int main(void)
{
    static const unsigned long v[] = { 0UL, 1UL, 0x80UL, 0xffUL, 0x1234UL,
                                       0x80000000UL, 0x7fffffffUL,
                                       0xdeadbeefUL, 0xffffffffUL,
                                       0x00ff00ffUL, 0xedb88320UL };
    int i;

    for (i = 0; i < (int) (sizeof v / sizeof v[0]); i++) {
        ops(v[i], (long) v[i]);
        deep(v[i]);
    }

    /* From gcc on the host, with long taken as 32 bits. */
    return h == 3934979774UL ? 42 : 1;
}
