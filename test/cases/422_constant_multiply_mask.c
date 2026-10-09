/* An int multiplied by a constant, and masked by one that clears its top
 * byte, as the machine IR makes them: the multiply by adds -- the routine
 * past eight of them, and for 0 and a negative -- and `x & 0x1ff` the two
 * low bytes masked and widened by zeros. Each constant on either side,
 * signed and unsigned, against the products and masks worked out. */

int sopaque[3] = { 1234, -77, 0 };
unsigned uopaque[2] = { 0xabcdef, 0x123456 };

static int sin_(int k) { return sopaque[k]; }
static unsigned uin(int k) { return uopaque[k]; }

static long products(int x)
{
    long sum = 0;

    sum += x * 0;
    sum += x * 1;
    sum += 2 * x;
    sum += x * 3;
    sum += x * 5;
    sum += 7 * x;
    sum += x * 10;
    sum += x * 20;
    sum += x * 60;
    sum += x * 100;
    sum += x * 255;
    sum += 256 * x;
    sum += x * 300;
    sum += x * -3;
    sum += x * -1;
    return sum;
}

static unsigned umasks(unsigned x)
{
    unsigned sum = 0;

    sum += x & 0x1ff;
    sum += 0xffff & x;
    sum += x & 0xff00;
    sum += x & 0x7f80;
    sum += x & 0x3ff;
    sum += (x + 1) & 511;
    return sum;
}

static int smasks(int x)
{
    return (x & 0xffff) + (x & 0x1ff) + (x & 0xff0);
}

#define CHECK(expr, want) { check++; if ((long) (expr) != (long) (want)) return check; }

int main(void)
{
    int check = 0;

    CHECK(products(sin_(0)), 1234L * (0 + 1 + 2 + 3 + 5 + 7 + 10 + 20 + 60 + 100 + 255 + 256 + 300 - 3 - 1))
    CHECK(products(sin_(1)), -77L * (0 + 1 + 2 + 3 + 5 + 7 + 10 + 20 + 60 + 100 + 255 + 256 + 300 - 3 - 1))
    CHECK(products(sin_(2)), 0)
    CHECK(umasks(uin(0)), 0x1ef + 0xcdef + 0xcd00 + 0x4d80 + 0x1ef + 0x1f0)
    CHECK(umasks(uin(1)), 0x56 + 0x3456 + 0x3400 + 0x3400 + 0x56 + 0x57)
    CHECK(smasks(sin_(1)), 0xffb3 + 0x1b3 + 0xfb0)
    CHECK(smasks(sin_(0)), 1234 + (1234 & 0x1ff) + (1234 & 0xff0))

    return 42;
}
