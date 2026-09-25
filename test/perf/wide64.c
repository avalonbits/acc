/* 64-bit arithmetic: modular exponentiation by squaring, a xorshift
 * generator, and division and remainder of long longs -- none of which
 * this machine does in fewer than a helper call. */
#include "perf.h"

static unsigned long long mulmod(unsigned long long a, unsigned long long b,
                                 unsigned long long m)
{
    return a * b % m;
}

static unsigned long long powmod(unsigned long long base, unsigned long long e,
                                 unsigned long long m)
{
    unsigned long long r = 1;

    base %= m;
    while (e) {
        if (e & 1)
            r = mulmod(r, base, m);
        base = mulmod(base, base, m);
        e >>= 1;
    }

    return r;
}

int main(void)
{
    unsigned long long x = perf_seed | 1, sum = 0;
    long long signed_sum = 0;

    perf_start();
    for (int i = 0; i < 40; i++) {
        x ^= x << 13;
        x ^= x >> 7;
        x ^= x << 17;
        sum += powmod(x, 65537, 4294967291ULL);
        signed_sum += (long long) (x >> 3) / (i + 3) - (long long) (x % 1000);
    }
    perf_stop();

    perf_check((sum ^ (unsigned long long) signed_sum) % 4000000000ULL);

    return 0;
}
