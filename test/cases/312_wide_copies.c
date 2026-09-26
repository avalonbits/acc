/* Longs and long longs copied between frame slots and set from constants,
 * which acc does three bytes at a time through IY where the slots are near
 * and a byte at a time where they are not: a long widened to a long long by
 * its sign and by zero, 8-byte copies whose last run overlaps the one
 * before it, constants of each width, and the same past the 128 bytes a
 * frame displacement reaches. */
static unsigned long h = 1;

static void mix(long v)
{
    h = (h * 31 + (unsigned long) v) & 0xffffffffUL;
}

static void mixll(long long v)
{
    mix((long) v);
    mix((long) (v >> 32));
}

static long long widen(long x) { long long y = x; return y; }
static unsigned long long uwiden(unsigned long x) { unsigned long long y = x; return y; }

/* Scalars enough that the last of them are past the 128 bytes a frame
 * displacement reaches, so that storing a constant or a copy there takes
 * the byte-at-a-time path. */
static long long past_reach(long long seed)
{
    long long v0 = seed, v1 = v0 + 1, v2 = v1 * 3, v3 = v2 - 7, v4 = v3 ^ v0,
              v5 = v4 + v1, v6 = v5 * 5, v7 = v6 - v2, v8 = v7 + v3,
              v9 = v8 ^ v4, v10 = v9 + v5, v11 = v10 - v6, v12 = v11 + v7,
              v13 = v12 * 3, v14 = v13 - v8, v15 = v14 + v9, v16, v17, v18;

    v16 = 0x0a0b0c0d0e0f1011LL;
    v17 = -0x0a0b0c0d0e0f1011LL;
    v18 = v15;
    mixll(v16); mixll(v17); mixll(v18);

    return v0 + v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 + v12
           + v13 + v14 + v15 + v16 + v17 + v18;
}

/* Wide constants, a scratch slot each, alive together: deep enough that
 * the statement's scratch goes past what a displacement reaches, and the
 * last of them are written a byte at a time. */
static unsigned long long far_consts(unsigned long long v, unsigned long long w)
{
    unsigned long long a0 = v, a1 = w, a2 = v + w, a3 = v - w, a4 = v * 2,
                       a5 = w * 2, a6 = v + 3, a7 = w + 3, a8 = v - 5,
                       a9 = w - 5, a10 = v ^ w, a11 = v | w;

    return a0 * (0x1111111111ULL + a1 * (0x2222222222ULL + a2 * (0x3333333333ULL
           + a3 * (0x4444444444ULL + a4 * (0x5555555555ULL + a5 * (0x6666666666ULL
           + a6 * (0x7777777777ULL + a7 * (0x0888888888ULL + a8 * (0x0999999999ULL
           + a9 * (a10 + a11))))))))));
}

static long far_side(long a, long long b)
{
    char pad[200];
    long x = a, y = -123456789L;
    long long z = b, w = 0x0102030405060708LL;
    int i;

    for (i = 0; i < 200; i++)
        pad[i] = (char) i;
    mix(x); mix(y); mixll(z); mixll(w);
    z = x;
    w = y;
    mixll(z); mixll(w);

    return x + y + pad[150];
}

int main(void)
{
    long a = -5, b = 70000L, c = 0x7fffffffL, d;
    unsigned long u = 0xfedcba98UL, v;
    long long p = -1, q = 0x123456789abcdefLL, r;
    unsigned long long s;

    d = a; mix(d);
    d = c; mix(d);
    v = u; mix((long) v);
    r = q; mixll(r);
    r = p; mixll(r);
    mixll(widen(a)); mixll(widen(b)); mixll(widen(c)); mixll(widen(-2147483647L - 1));
    s = uwiden(u); mixll((long long) s);
    s = uwiden(0x80000000UL); mixll((long long) s);
    r = 0x0011223344556677LL; mixll(r);
    r = -0x0011223344556677LL; mixll(r);
    d = 0x00ff00ffL; mix(d);
    d = -0x00ff00ffL; mix(d);
    r = a; mixll(r);
    r = u; mixll(r);
    mix(far_side(b, q));
    mix(far_side(a, p));
    mixll(past_reach(q));
    mixll(past_reach(-3));
    mixll((long long) far_consts(12345, 678));
    mixll((long long) far_consts(0xffffffffffULL, 3));

    /* From gcc on the host. */
    return h == 2607731625UL ? 42 : 1;
}
