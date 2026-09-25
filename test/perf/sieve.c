/* The sieve of Eratosthenes over 16,000 bytes, three times, counting the
 * primes: bytes written and read in strided loops. */
#include "perf.h"

#define LIMIT 16000

static char composite[LIMIT];

int main(void)
{
    unsigned limit = LIMIT - (unsigned) (perf_seed & 1);
    unsigned long check = 0;

    perf_start();
    for (int pass = 0; pass < 3; pass++) {
        unsigned count = 0;

        for (unsigned i = 0; i < limit; i++)
            composite[i] = 0;
        for (unsigned i = 2; i < limit; i++) {
            if (composite[i])
                continue;
            count++;
            for (unsigned j = i + i; j < limit; j += i)
                composite[j] = 1;
        }
        check = check * 3 + count;
    }
    perf_stop();

    perf_check(check);

    return 0;
}
