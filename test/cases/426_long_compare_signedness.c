/* A long compared with an int, or two longs, signed as C's conversions
 * make the pair: an int and an unsigned long unsigned -- a negative int as
 * a large one -- an unsigned int and a long signed, the long holding all
 * the unsigned int can be, and two longs unsigned where either is. The
 * machine IR took the left operand's type alone, and compared `x <
 * 0x900000UL` signed where x was a negative int. */

int ints[4] = { -8388608, -1, 0, 8388607 };
unsigned uints[2] = { 0, 0xffffff };
long longs[3] = { -5L, 0L, 0x7fffffffL };
unsigned long ulongs[2] = { 5UL, 0xfffffff0UL };
signed char chars[1] = { -3 };

__attribute__((noinline)) static int int_ulong(int x, unsigned long u)
{
    return (x < 0x900000UL) + 2 * (x > 1UL) + 4 * (x <= u) + 8 * (u >= x)
           + 16 * (x != 0x7ffffffeUL);
}

__attribute__((noinline)) static int uint_long(unsigned w, long l)
{
    return (w < l) + 2 * (w > -1L) + 4 * (l <= w) + 8 * (w != l);
}

__attribute__((noinline)) static int long_ulong(long l, unsigned long u)
{
    return (l < u) + 2 * (u > l) + 4 * (l >= u) + 8 * (l < 1UL);
}

__attribute__((noinline)) static int char_ulong(signed char c)
{
    return (c < 10UL) + 2 * (c > 0x80000000UL);
}

#define CHECK(expr, want) { check++; if ((long) (expr) != (long) (want)) return check; }

int main(void)
{
    int check = 0;

    CHECK(int_ulong(ints[0], ulongs[0]) + 32 * int_ulong(ints[1], ulongs[1]), 594)
    CHECK(int_ulong(ints[2], ulongs[0]) + 32 * int_ulong(ints[3], ulongs[1]), 1021)
    CHECK(uint_long(uints[0], longs[0]) + 16 * uint_long(uints[1], longs[2]), 190)
    CHECK(uint_long(uints[1], longs[1]), 14)
    CHECK(long_ulong(longs[0], ulongs[0]) + 16 * long_ulong(longs[2], ulongs[1]), 52)
    CHECK(char_ulong(chars[0]), 2)

    return 42;
}
