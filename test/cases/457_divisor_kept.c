/* A long or long long divided, or its remainder taken, by a variable read
 * where it is -- and a float taken away by one: the right operand left as
 * it was, which acc now reads in place rather than copying first. Signed
 * divisors made positive in a copy, not in the variable; a constant read
 * from the pool, twice, the second time the same. */
static long ldiv_(long a, long b, long *kept) { long q = a / b; *kept = b; return q; }
static long lrem_(long a, long b, long *kept) { long r = a % b; *kept = b; return r; }
static unsigned long udiv_(unsigned long a, unsigned long b, unsigned long *kept)
{
    unsigned long q = a / b;

    *kept = b;
    return q;
}
static unsigned long urem_(unsigned long a, unsigned long b, unsigned long *kept)
{
    unsigned long r = a % b;

    *kept = b;
    return r;
}
static long long lldiv_(long long a, long long b, long long *kept)
{
    long long q = a / b;

    *kept = b;
    return q;
}
static long long llrem_(long long a, long long b, long long *kept)
{
    long long r = a % b;

    *kept = b;
    return r;
}
static float fsub_(float a, float b, float *kept) { float d = a - b; *kept = b; return d; }
static unsigned long twice(unsigned long a) { return a % 65521UL + a % 65521UL; }

int main(void)
{
    long kl;
    unsigned long ku;
    long long kll;
    float kf;

    if (ldiv_(-100000L, -7L, &kl) != 14285L || kl != -7L)
        return 1;
    if (lrem_(-100000L, 7L, &kl) != -5L || kl != 7L)
        return 2;
    if (lrem_(100000L, -7L, &kl) != 5L || kl != -7L)
        return 3;
    if (udiv_(4000000000UL, 3UL, &ku) != 1333333333UL || ku != 3UL)
        return 4;
    if (urem_(4000000000UL, 7UL, &ku) != 4000000000UL % 7UL || ku != 7UL)
        return 5;
    if (lldiv_(-100000000000LL, -9LL, &kll) != 11111111111LL || kll != -9LL)
        return 6;
    if (llrem_(-100000000000LL, 9LL, &kll) != -1LL || kll != 9LL)
        return 7;
    if (fsub_(10.5f, 2.25f, &kf) != 8.25f || kf != 2.25f)
        return 8;
    if (twice(70000UL) != 2UL * (70000UL % 65521UL))
        return 9;

    return 42;
}
