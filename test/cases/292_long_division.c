/* Dividing longs, unsigned and signed, quotient and remainder: the edges
 * of the shift-and-subtract loop -- divisors past 2^31, a dividend below, at
 * and one short of the divisor, 1 and 0xffffffff either side -- and two
 * hundred pairs from a generator, divisors of every width.
 * Dividends of one, two, three and four bytes, since the loop starts at
 * the first byte that is not zero -- reading the bytes below it from before
 * the dividend, where acc keeps the divisor's top bytes, and clearing
 * them: the last two pairs put bits there. The answer is a checksum worked out in
 * Python with 32-bit arithmetic,
 * and truncating division for the signed ones, as C99 has it. */
static const unsigned long edges[][2] = {
    { 0xffffffffUL, 1 }, { 0xffffffffUL, 0xffffffffUL },
    { 0x80000000UL, 0x80000001UL }, { 0xfffffffeUL, 0x80000000UL },
    { 100, 0x90000000UL }, { 0x7fffffffUL, 3 }, { 12345, 12345 },
    { 12344, 12345 }, { 0, 7 }, { 0xdeadbeefUL, 0x10000UL },
    { 0x123456UL, 7 }, { 0xffffffUL, 0x10 }, { 0x10000UL, 0xff }, { 0x100, 3 },
    { 0x123456UL, 0x81000000UL }, { 0x1234, 0xff00ff00UL },
};

static unsigned long check;

static void divide(unsigned long a, unsigned long b)
{
    long sa = (long) a, sb = (long) b;

    check = check * 31 + a / b;
    check = check * 31 + a % b;
    check = check * 31 + (unsigned long) (sa / sb);
    check = check * 31 + (unsigned long) (sa % sb);
}

int main(void)
{
    unsigned long state = 12345;

    for (int i = 0; i < 16; i++)
        divide(edges[i][0], edges[i][1]);
    for (int i = 0; i < 200; i++) {
        unsigned long a, b;

        state = state * 1103515245UL + 12345UL;
        a = state;
        state = state * 1103515245UL + 12345UL;
        b = state >> (state & 31);
        if (b == 0)
            b = 1;
        divide(a, b);
    }

    return check == 3960399474UL ? 42 : 1;
}
