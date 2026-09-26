/* Wide values worked out through the runtime's routines and assigned to a
 * variable, which acc works out in the variable itself: one operator and
 * chains of them, starting from the variable or from another, and the
 * cases it must not -- a right operand that is the variable, the variable
 * on both sides -- and the value of the assignment used again, in longs
 * and long longs. Floats are test/self/031's: the host's float routines
 * are not the Agon's. */
static unsigned long h = 1;

static void mix(unsigned long v)
{
    h = ((h ^ (h >> 13)) * 31 + v) & 0xffffffffUL;
}

static void mixll(unsigned long long v)
{
    mix((unsigned long) v);
    mix((unsigned long) (v >> 32));
}

static void longs(unsigned long a, unsigned long b)
{
    unsigned long x = a, y = b, z;

    x = x * 69069UL + 1UL;              mix(x);
    x = x * 3UL + y * 5UL;              mix(x);
    y = x * 7UL;                        mix(y);
    x = y + x;                          mix(x);
    x = x + x;                          mix(x);
    x = x * x - y;                      mix(x);
    z = y = x * 11UL + 13UL;            mix(y); mix(z);
    x = ((x * 3UL + 5UL) * 7UL + 11UL) * 13UL;  mix(x);
    x = b - x;                          mix(x);
    x = x / 7UL;                        mix(x);
    x = y % (x | 1UL);                  mix(x);
    y *= 3UL;                           mix(y);
    y += x * 9UL;                       mix(y);
}

static void quads(unsigned long long a)
{
    unsigned long long q = a, r = a ^ 0x5555ULL;

    q = q * 6364136223846793005ULL + 1442695040888963407ULL;  mixll(q);
    r = q * 3ULL + r;                                        mixll(r);
    q = r - q;                                               mixll(q);
}

int main(void)
{
    static const unsigned long v[] = { 0UL, 1UL, 12345UL, 0x7fffffffUL, 0xffffffffUL };
    int i;

    for (i = 0; i < 5; i++) {
        longs(v[i], v[(i + 1) % 5]);
        quads((unsigned long long) v[i] << 7);
    }

    /* From gcc on the host, with long taken as 32 bits. */
    return h == 3303730920UL ? 42 : 1;
}
