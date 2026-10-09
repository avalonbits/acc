/* An int, or something narrower, compared with a long constant: made at 24
 * bits where the constant fits what the value can be -- unsigned where the
 * value is never negative, or the comparison unsigned and the constant
 * below 2^23, signed where both are -- and as longs where it does not.
 * Each kind of value against constants at the edges, and past them. */

unsigned useed[4] = { 0, 65520, 65521, 0xffffff };
int sseed[4] = { -8388608, -1, 0, 8388607 };
unsigned char cseed[2] = { 0, 255 };
signed char scseed[2] = { -128, 127 };
unsigned short hseed[2] = { 0, 65535 };
short sseed16[2] = { -32768, 32767 };

__attribute__((noinline)) static int u24(unsigned w)
{
    return (w > 65520UL) + 2 * (w <= 0xffffffUL) + 4 * (w == 0UL) + 8 * (w != 65521L)
           + 16 * (w < 0x1000000L) + 32 * (w >= 1UL);
}

__attribute__((noinline)) static int s24(int x)
{
    return (x < 0L) + 2 * (x > -8388608L) + 4 * (x <= 8388607L) + 8 * (x >= -1L)
           + 16 * (x < 100UL) + 32 * (x == 8388607L) + 64 * (x > 8388608L)
           + 128 * (x != -8388609L) + 256 * (x < 0x900000UL);
}

__attribute__((noinline)) static int narrow(unsigned char c, signed char s, unsigned short h, short i)
{
    return (c > 254L) + 2 * (c != 300L) + 4 * (s < 0L) + 8 * (s < 100UL)
           + 16 * (h >= 65535UL) + 32 * (i < -32767L) + 64 * (i > 0UL)
           + 128 * (h == 65535L);
}

#define CHECK(expr, want) { check++; if ((long) (expr) != (long) (want)) return check; }

int main(void)
{
    int check = 0;

    CHECK(u24(useed[0]) + 64 * u24(useed[1]) + 4096 * u24(useed[2]), 212638)
    CHECK(u24(useed[3]), 59)
    CHECK(s24(sseed[0]) + 512 * s24(sseed[1]), 73349)
    CHECK(s24(sseed[2]) + 512 * s24(sseed[3]), 220574)
    CHECK(narrow(cseed[0], scseed[0], hseed[0], sseed16[0]), 102)
    CHECK(narrow(cseed[1], scseed[1], hseed[1], sseed16[1]), 211)

    return 42;
}
