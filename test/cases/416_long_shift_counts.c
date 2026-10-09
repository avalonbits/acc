/* Longs shifted right by every count from 0 to 31, signed and unsigned,
 * the count not known as it is compiled: the runtime's routines that take
 * a long in registers, which move whole bytes before the bits that are
 * left. */

static volatile int zero;

static const unsigned long vals[] = {
    0x0UL, 0x1UL, 0xffffffffUL, 0x80000000UL, 0x7fffffffUL, 0x12345678UL, 0xedcba987UL, 0xff00ff00UL, 0xff00ffUL, 0x80808080UL, 0x1010101UL
};

static long shrs(long v, int k)
{
    return v >> k;
}

static unsigned long shru(unsigned long v, int k)
{
    return v >> k;
}

int main(void)
{
    unsigned long sum = 0;
    int i, k;

    for (i = 0; i < (int) (sizeof vals / sizeof vals[0]); i++)
        for (k = zero; k < 32; k++) {
            sum = sum * 31 + (unsigned long) shrs((long) vals[i], k);
            sum = sum * 31 + shru(vals[i], k);
        }

    return sum == 3470766742UL ? 42 : 1;
}
