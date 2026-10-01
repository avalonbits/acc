/* Longs and floats in functions the leaf backend makes: each value wider
 * than an int in a frame slot of its own, and each instruction with one in
 * it made by the first pass's code -- arithmetic, shifts, comparisons,
 * conversions both ways, steps, reads and writes through pointers -- the
 * rest made here around them. */
static int hash(const unsigned char *p, int n)
{
    unsigned long sum = 0;

    for (int i = 0; i < n; i++)
        sum = sum * 31 + p[i];

    return (int) (sum >> 8) & 0x7fff;
}

static int count_big(const long *v, int n, long limit)
{
    int count = 0;

    while (n--)
        if (*v++ > limit)
            count++;

    return count;
}

static int steps(void)
{
    long a = -3;
    unsigned long b = 0xfffffffeUL;

    a++;
    ++a;
    b++;
    b += 1;

    return (int) a * 10 + (b == 0);
}

static int store(long *dst, int n)
{
    for (int i = 0; i < n; i++)
        dst[i] = (long) i * 100000L - 50000L;

    return (int) (dst[n - 1] / 1000);
}

/* A long member read through a pointer the leaf backend keeps in IY, and
 * compared with a long parameter: all four of its bytes. */
struct holder { int id; char tag; long big; };

static int same_big(const struct holder *h, long big)
{
    return h->big == big;
}

/* An int written through a pointer to a long, both worked out: the two
 * on the stack here, and the store the first pass's code. */
static void put(long *base, int i, int v)
{
    *(base + i * 2) = v * 3 - 1;
}

/* Long and float arithmetic in a loop: multiply, divide, shifts by a
 * variable, a remainder, and a float worked from an int. */
static int mixed(int n, long k)
{
    long s = 0;
    float f = 1.0f;

    for (int i = 0; i < n; i++) {
        s += (long) i * k / 3 + (k >> i) + (k << (i & 7)) % 1000;
        f = f * 1.5f - (float) i;
    }

    return (int) (s & 0x7fff) + (int) f;
}

static int floats(int n)
{
    float x = 0.5f;

    for (int i = 0; i < n; i++)
        x = x * 1.5f + 0.25f;

    return (int) (x * 4.0f);
}

int main(void)
{
    static const unsigned char text[] = "wide values in the leaf backend";
    static const long values[5] = { -70000L, 3L, 70000L, 2000000L, -1L };
    static long out[4];
    int right = 0;

    right += hash(text, sizeof text - 1) == 0x7956;
    right += count_big(values, 5, 0L) == 3 && count_big(values, 5, 69999L) == 2;
    right += steps() == -9;
    right += store(out, 4) == 250 && out[0] == -50000L && out[2] == 150000L;
    right += floats(3) == 11;
    {
        static const struct holder h = { 3, 'q', 100000L };

        right += same_big(&h, 100000L) && !same_big(&h, 100001L)
                 && !same_big(&h, 100000L + 0x1000000L);
    }
    {
        static long out[8];

        put(out, 2, 7);
        right += out[4] == 20 && out[2] == 0;
    }
    right += (mixed(6, 100000L) & 0xff) == 23;
    return right == 8 ? 42 : right;
}
