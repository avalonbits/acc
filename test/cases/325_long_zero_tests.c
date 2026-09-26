/* Longs and long longs tested against zero, which acc does by ORing their
 * bytes in A and branching on Z: == 0, != 0, if, !, &&, ||, while and ?:,
 * over values with one byte set in each place, from variables and from
 * values worked out, and where so many temporaries are alive that the
 * bytes are out of reach -- one of them zero, so that bytes read from the
 * wrong place would tell. And constants on the left of + * & | ^, which go
 * to the right. */
static unsigned long h = 1;

static void mix(unsigned long v)
{
    h = ((h ^ (h >> 13)) * 31 + v) & 0xffffffffUL;
}

static void tests(unsigned long x, unsigned long long q)
{
    unsigned long n = 0;

    mix(x == 0); mix(x != 0); mix(!x); mix(q == 0); mix(q != 0); mix(!q);
    if (x)
        mix(3);
    if (!q)
        mix(5);
    mix(x && q); mix(x || q); mix((x & 1UL) ? 7 : 9); mix((q & 0x100000000ULL) ? 11 : 13);
    while (x) {
        x >>= 8;
        n++;
    }
    mix(n);
    mix(1000UL + x); mix(3UL * x); mix(0xf0f0UL & x); mix(0x8000UL | x);
    mix(0xffUL ^ x); mix((unsigned long) (7ULL * q)); mix((unsigned long) (0xffULL & q));
}

static void deep(unsigned long v)
{
    unsigned long x1 = v + 1, x2 = v + 2, x3 = v + 3, x4 = v + 4, x5 = v +
                  5, x6 = v + 6, x7 = v + 7, x8 = v + 8, x9 = v + 9, x10 = v
                  + 10, x11 = v + 11, x12 = v + 12, x13 = v + 13, x14 = v +
                  14, x15 = v + 15, x16 = v + 16, x17 = v + 17, x18 = v +
                  18, x19 = v + 19, x20 = v + 20, x21 = v + 21, x22 = v +
                  22, x23 = v + 23, x24 = v + 24;
    mix(x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10 + x11 + x12 + x13
        + x14 + x15 + x16 + x17 + x18 + x19 + x20 + x21 + x22 + x23 + x24);
    mix((x1 ^ x2) + ((x3 ^ x4) + ((x5 ^ x6) + ((x7 ^ x8)
        + ((x9 ^ x10) + ((x11 ^ x12) + ((x13 ^ x14) + ((x15 ^ x16)
        + ((((x17 ^ x18) + (x19 | x20)) - ((x19 | x20) + (x17 ^ x18))) != 0)))))))));
    mix((x1 ^ x2) + ((x3 ^ x4) + ((x5 ^ x6) + ((x7 ^ x8)
        + ((x9 ^ x10) + ((x11 ^ x12) + ((x13 ^ x14) + ((x15 ^ x16)
        + (((x17 & x18) ^ ((x19 | x20) & ((x21 | x22) & (x23 | x24)))) != 0)))))))));
    mix(((unsigned long long) v != 0) + ((unsigned long long) (v & 0xffUL) == 0));
}

int main(void)
{
    int i;

    tests(0, 0);
    for (i = 0; i < 32; i++)
        tests(1UL << i, 1ULL << (i + i));
    tests(0xffffffffUL, 0xffffffffffffffffULL);
    for (i = 0; i < 8; i++)
        deep((unsigned long) i * 0x01010101UL);

    /* From gcc on the host, with long taken as 32 bits. */
    return h == 3641279616UL ? 42 : 1;
}
