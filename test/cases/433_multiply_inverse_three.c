/* A multiply by 0xaaaaab, the inverse of 3 modulo 2^24, which a pointer
 * difference on three-byte elements is: a routine of its own, shifts and
 * adds, where it was the general multiply. Every value it takes, not only
 * multiples of 3 -- it is a multiply, wrapping at twenty-four bits -- over
 * the whole range, signed and unsigned; and the difference it divides. */

int ints[64];

__attribute__((noinline)) int times_signed(int x)
{
    return x * -5592405;
}

__attribute__((noinline)) unsigned times_unsigned(unsigned x)
{
    return x * 0xaaaaabu;
}

__attribute__((noinline)) int apart(int *p, int *q)
{
    return p - q;
}

/* x * 0xaaaaab the long way, a bit at a time, in 24 bits. */
unsigned reference(unsigned x)
{
    unsigned m = 0xaaaaabu, sum = 0;

    while (m) {
        if (m & 1)
            sum += x;
        x <<= 1;
        m >>= 1;
    }

    return sum & 0xffffffu;
}

int main(void)
{
    unsigned x = 1;
    int k, i;

    /* 2000 values from a multiplicative walk, which reaches every byte. */
    for (k = 0; k != 2000; k++) {
        x = (x * 1103515u + 12345u) & 0xffffffu;
        if (times_unsigned(x) != reference(x))
            return 1;
        if ((unsigned) times_signed((int) x) != reference(x))
            return 2;
    }
    if (times_unsigned(0) != 0 || times_unsigned(3) != 1 || times_unsigned(0xfffffdu) != 0xffffffu)
        return 3;
    if (times_signed(0x7fffff) != (int) reference(0x7fffffu)
        || times_signed(-0x800000) != (int) reference(0x800000u))
        return 4;
    for (i = 0; i != 64; i++)
        for (k = 0; k < 64; k += 7)
            if (apart(&ints[i], &ints[k]) != i - k)
                return 5;

    return 42;
}
