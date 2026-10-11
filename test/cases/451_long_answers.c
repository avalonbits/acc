/* A call's long answer, which comes back in E:HL: stored straight into
 * the variable it is assigned to, where it was stored to scratch and
 * copied, and returned straight from E:HL, where it was stored and loaded
 * again -- as a long and as a float, and used after the store, and
 * stored into a variable that is then returned, and given to another
 * call. */
static long twice(long x) { return x * 2; }
static float half(float x) { return x / 2.0f; }
static unsigned long big(int x) { return 3000000000UL + (unsigned long) x; }

static long direct(long x) { return twice(x); }
static float fdirect(float x) { return half(x); }

static long stored(long x)
{
    long r;

    r = twice(x);
    return r;
}

static long used(long x)
{
    long r = twice(x), s;

    s = twice(r);
    r = r + 1;
    return r + s;
}

static long either(int c, long x)
{
    long r;

    r = c ? twice(x) : twice(x + 1);    /* two answers, one slot, a join */
    return r;
}

static long choose(int c, long x)
{
    return c ? twice(x) : twice(-x);
}

static long both(long x)
{
    long a, b;

    a = b = twice(x);                   /* moved into b, then a copy of b */
    return a + b;
}

static float stale(long x, int i)
{
    float f;

    twice(x);                           /* an answer left in scratch */
    f = (float) i;                      /* made in the same slot after */
    return f;
}

static unsigned long ubig(int x)
{
    unsigned long u = big(x);

    return u;
}

int main(void)
{
    float f;

    if (direct(100000) != 200000 || stored(-70000) != -140000)
        return 1;
    if (used(5) != 11 + 20)
        return 2;
    if (ubig(7) != 3000000007UL)
        return 3;
    f = half(9.0f);
    if (f != 4.5f || fdirect(3.0f) != 1.5f)
        return 4;
    if (twice(twice(3)) != 12)
        return 5;
    if (either(1, 10) != 20 || either(0, 10) != 22)
        return 6;
    if (choose(1, 4) != 8 || choose(0, 4) != -8)
        return 7;
    if (both(5) != 20)
        return 8;
    if (stale(1000, 3) != 3.0f)
        return 9;

    return 42;
}
