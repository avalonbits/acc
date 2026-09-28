/* long long and unsigned long long: eight bytes, which is wider than the
 * frame slots every other value fits in. Arithmetic, the shifts and the
 * comparisons, conversion to and from the narrower types and to and from a
 * float, arguments and results, globals, arrays, members, a switch and
 * va_arg. */
#include <stdarg.h>

long long g = 0x1122334455667788LL;
unsigned long long ug = 18446744073709551615ULL;
long long table[3] = { -1LL, 0x7fffffffffffffffLL, 1LL };

struct box { char tag; long long v; };

long long add(long long a, long long b) { return a + b; }
unsigned long long umul(unsigned long long a, unsigned long long b) { return a * b; }
long long tri(long long a, int b, long long c) { return a * b - c; }

int digits(long long v) {
    int n = 0;

    while (v) {
        v /= 10;
        n++;
    }

    return n;
}

int pick(long long v) {
    switch (v) {
    case 0x100000000LL:  return 1;
    case -5LL:           return 2;
    case 7LL:            return 3;
    }

    return 0;
}

long long sum(int n, ...) {
    va_list ap;
    long long s = 0;

    va_start(ap, n);
    while (n--)
        s += va_arg(ap, long long);
    va_end(ap);

    return s;
}

int main(void) {
    long long a = 1000000000000LL, b = -3LL;
    unsigned long long u = 0xffffffffffffffffULL;
    struct box box;
    float f;
    int i;

    if (sizeof(long long) != 8 || sizeof(unsigned long long) != 8)
        return 1;
    if (a + b != 999999999997LL || a - b != 1000000000003LL)
        return 2;
    if (a * 3LL != 3000000000000LL || a / 7LL != 142857142857LL)
        return 3;
    if (a % 7LL != 1LL || b % 2LL != -1LL || b / 2LL != -1LL)
        return 4;
    if ((a << 20) != 1048576000000000000LL || (a >> 20) != 953674LL)
        return 5;
    if ((b >> 1) != -2LL || ((unsigned long long) b >> 60) != 15ULL)
        return 6;
    if ((a & 0xffffLL) != 4096LL || (a | 1LL) != 1000000000001LL)
        return 7;
    if ((a ^ a) != 0LL || ~0LL != -1LL || -a != -1000000000000LL)
        return 8;
    if (!(b < a) || !(a > b) || b >= a || a <= b || a == b || !(a != b))
        return 9;
    if (!(u > 1ULL) || (long long) u >= 0LL)
        return 10;
    if (u + 1ULL != 0ULL || u / 3ULL != 6148914691236517205ULL)
        return 11;
    if (g != 0x1122334455667788LL || (int) (g >> 56) != 0x11)
        return 12;
    if (ug != u || table[0] != -1LL || table[1] - 1LL != 0x7ffffffffffffffeLL)
        return 13;
    if (add(a, b) != 999999999997LL || umul(4000000000ULL, 4000000000ULL)
        != 16000000000000000000ULL)
        return 14;
    if (tri(a, 3, 1LL) != 2999999999999LL)
        return 15;
    if (digits(a) != 13 || digits(0LL) != 0)
        return 16;
    if (pick(0x100000000LL) != 1 || pick(-5LL) != 2 || pick(7LL) != 3
        || pick(8LL) != 0)
        return 17;
    if (sum(3, 1LL, 2LL, 0x100000000LL) != 0x100000003LL)
        return 18;

    /* Conversions: to and from the narrower integers, and to and from a
     * float, which rounds what a long long does not fit in. */
    i = (int) a;
    if ((long long) i != -5959680LL)    /* the low 24 bits, as an int */
        return 19;
    if ((long long) (char) -1 != -1LL || (long long) (unsigned char) 255 != 255LL)
        return 20;
    if ((long) (a >> 16) != 15258789L || (unsigned) (a & 0xffffff) != 10817536u)
        return 21;
    f = (float) a;
    if (f != 1.0e12f || (long long) f != 999999995904LL)
        return 22;
    if ((float) u != 18446744073709551616.0f || (float) (long long) -1 != -1.0f)
        return 23;
    if ((long long) 1.0e18f != 999999984306749440LL)
        return 24;

    box.tag = 'q';
    box.v = a;
    if (box.v != a || box.tag != 'q' || sizeof box != 9)
        return 25;
    a++;
    --a;
    a += 2LL;
    if (a != 1000000000002LL)
        return 26;
    if ((b < 0LL ? a : b) != a || (1 ? 1LL : 2) != 1LL)
        return 27;

    return 42;
}
