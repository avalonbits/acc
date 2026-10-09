/* Divides and remainders by constants, as the machine IR makes them: by 1
 * the value itself, or 0; unsigned by a power of two a shift right, or the
 * bits below it -- signed, and by anything else, the routines. The
 * constant given directly or through a local set to it, `const unsigned a
 * = 1`, which is the constant wherever it is read: `(size + a - 1) / a *
 * a`, the round-up ez80asm makes with an offsetof that is 1 here. */

unsigned useed[3] = { 0xabcdef, 1000, 7 };
int sseed[3] = { -1000, 123456, -7 };

static unsigned uin(int k) { return useed[k]; }
static int sin_(int k) { return sseed[k]; }

__attribute__((noinline)) static unsigned unsigned_ops(unsigned x)
{
    const unsigned one = 1, eight = 8;
    unsigned sum = 0;

    sum += x / 1 + x % 1;
    sum += x / 2 + x % 2;
    sum += x / 4 + x % 4;
    sum += x / eight + x % eight;
    sum += x / 256 + x % 256;
    sum += x / 65536 + x % 65536;
    sum += x / 0x800000 + x % 0x800000;
    sum += x / 3 + x % 3;
    sum += ((x + one - 1) / one) * one;
    sum += ((x + eight - 1) / eight) * eight;
    return sum;
}

__attribute__((noinline)) static int signed_ops(int x)
{
    const int one = 1;
    int sum = 0;

    sum += x / 1 + x % 1;
    sum += x / one + x % one;
    sum += x / 2 + x % 2;
    sum += x / 16 + x % 16;
    sum += x / 3 + x % 3;
    return sum;
}

/* Locals set to constants each converts: what they are read as. */
__attribute__((noinline)) static long narrow_consts(int x)
{
    const signed char neg = (signed char) 254;
    const unsigned char big = (unsigned char) -56;
    const short sh = (short) 40000;
    const unsigned short ush = (unsigned short) -5536;
    const _Bool yes = 256;
    const int wide = 0x1234567;

    return (long) (x + neg) * 100000L + (x + big) * 1000L + sh + ush + yes + wide;
}

__attribute__((noinline)) static unsigned char byte_rem(unsigned x)
{
    return (unsigned char) (x % 128);
}

#define CHECK(expr, want) { check++; if ((long) (expr) != (long) (want)) return check; }

int main(void)
{
    int check = 0;

    CHECK(unsigned_ops(uin(0)), 19443)
    CHECK(unsigned_ops(uin(1)), 6444)
    CHECK(unsigned_ops(uin(2)), 61)
    CHECK(signed_ops(sin_(0)), -2904)
    CHECK(signed_ops(sin_(1)), 357508)
    CHECK(signed_ops(sin_(2)), -28)
    CHECK(byte_rem(uin(0)), 111)
    CHECK(narrow_consts(sin_(2)), 1638992L)

    return 42;
}
