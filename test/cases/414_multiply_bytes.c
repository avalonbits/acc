/* The 24-bit multiply, acc_rt_mul, over the operands whose bytes reach
 * each partial product: zero, one, the extremes, each byte alone, and a
 * run of pseudo-random pairs -- the products summed, as agondev's are. */

static volatile int seed = 12345;

static int mul(int a, int b)
{
    return a * b;
}

static unsigned umul(unsigned a, unsigned b)
{
    return a * b;
}

int main(void)
{
    static const int edge[] = {
        0, 1, -1, 2, -2, 255, 256, 257, 65535, 65536, 65537,
        0x7fffff, -0x800000, 0x123456, -0x123456, 0xff00, 0xff0000 - 0x1000000,
        0x00ff00, 0x0000ff, 0x010101, 0x808080 - 0x1000000, 3, 100, 1000
    };
    unsigned long sum = 0;
    unsigned state = (unsigned) seed;
    int i, j;

    for (i = 0; i < (int) (sizeof edge / sizeof edge[0]); i++)
        for (j = 0; j < (int) (sizeof edge / sizeof edge[0]); j++)
            sum = sum * 31 + (unsigned long) (mul(edge[i], edge[j]) & 0xffffff);
    for (i = 0; i < 500; i++) {
        unsigned a, b;

        state = state * 1103515245u + 12345u;
        a = state;
        state = state * 1103515245u + 12345u;
        b = state;
        sum = sum * 31 + (unsigned long) (umul(a, b) & 0xffffff);
    }

    return sum == 4103166528UL ? 42 : 1;
}
