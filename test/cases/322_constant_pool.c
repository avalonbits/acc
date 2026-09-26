/* Wide constants that routines read, which acc lays down once after a
 * function's code and loads the address of: the same constant many times,
 * more of them than the pool holds, comparisons with the constant on either
 * side, long and long long, and uses of the pool that are taken back --
 * inside a sizeof, in a for loop's step compiled in its place and then
 * again after the body, down both ways of a ?: -- and a static function
 * nothing calls, whose pool goes with it. */
static unsigned long h = 1;

static void mix(unsigned long v)
{
    h = ((h ^ (h >> 13)) * 31 + v) & 0xffffffffUL;
}

static unsigned long unused(unsigned long x)
{
    return x * 123456789UL + 987654321UL;
}

static unsigned long many(unsigned long x)
{
    x = x * 17UL + 17UL;
    mix(x);
    x = x * 1000020UL + 7000038UL;
    mix(x);
    x = x * 2000023UL + 14000059UL;
    mix(x);
    x = x * 3000026UL + 21000080UL;
    mix(x);
    x = x * 4000029UL + 28000101UL;
    mix(x);
    x = x * 5000032UL + 35000122UL;
    mix(x);
    x = x * 6000035UL + 2000023UL;
    mix(x);
    x = x * 7000038UL + 9000044UL;
    mix(x);
    x = x * 8000041UL + 16000065UL;
    mix(x);
    x = x * 9000044UL + 23000086UL;
    mix(x);
    x = x * 10000047UL + 30000107UL;
    mix(x);
    x = x * 11000050UL + 37000128UL;
    mix(x);
    x = x * 12000053UL + 4000029UL;
    mix(x);
    x = x * 13000056UL + 11000050UL;
    mix(x);
    x = x * 14000059UL + 18000071UL;
    mix(x);
    x = x * 15000062UL + 25000092UL;
    mix(x);
    x = x * 16000065UL + 32000113UL;
    mix(x);
    x = x * 17000068UL + 39000134UL;
    mix(x);
    x = x * 18000071UL + 6000035UL;
    mix(x);
    x = x * 19000074UL + 13000056UL;
    mix(x);
    x = x * 20000077UL + 20000077UL;
    mix(x);
    x = x * 21000080UL + 27000098UL;
    mix(x);
    x = x * 22000083UL + 34000119UL;
    mix(x);
    x = x * 23000086UL + 1000020UL;
    mix(x);
    x = x * 24000089UL + 8000041UL;
    mix(x);
    x = x * 25000092UL + 15000062UL;
    mix(x);
    x = x * 26000095UL + 22000083UL;
    mix(x);
    x = x * 27000098UL + 29000104UL;
    mix(x);
    x = x * 28000101UL + 36000125UL;
    mix(x);
    x = x * 29000104UL + 3000026UL;
    mix(x);
    x = x * 30000107UL + 10000047UL;
    mix(x);
    x = x * 31000110UL + 17000068UL;
    mix(x);
    x = x * 32000113UL + 24000089UL;
    mix(x);
    x = x * 33000116UL + 31000110UL;
    mix(x);
    x = x * 34000119UL + 38000131UL;
    mix(x);
    x = x * 35000122UL + 5000032UL;
    mix(x);
    x = x * 36000125UL + 12000053UL;
    mix(x);
    x = x * 37000128UL + 19000074UL;
    mix(x);
    x = x * 38000131UL + 26000095UL;
    mix(x);
    x = x * 39000134UL + 33000116UL;
    mix(x);

    return x;
}

static void compares(long a, long long q)
{
    mix(a < 100000L); mix(a > 100000L); mix(100000L < a); mix(100000L >= a);
    mix(a <= -70000L); mix(a == 123456L); mix(a != 123456L);
    mix(q < 5000000000LL); mix(q > -5000000000LL); mix(q == 42LL);
    mix(5000000000LL <= q);
}

int main(void)
{
    static const long v[] = { 0L, 1L, -1L, 99999L, 100000L, 100001L,
                              123456L, -70000L, -70001L, 0x7ffffff0L };
    unsigned long x = 1, y;
    long long q;
    int i;

    for (i = 0; i < (int) (sizeof v / sizeof v[0]); i++) {
        q = (long long) v[i] * 100000LL;
        compares(v[i], q);
    }
    mix(many(7UL));
    mix((unsigned long) sizeof(x * 1234567UL + 7654321UL));
    for (y = 1; y < 1000000UL; y = y * 3UL + 1UL)
        mix(y);
    for (i = 0; i < 4; i++) {
        x = i & 1 ? x * 1103515245UL + 12345UL : x * 69069UL + 1UL;
        mix(x);
    }
    mix(x * 1103515245UL);

    /* From gcc on the host, with long taken as 32 bits. */
    return h == 2433181869UL ? 42 : 1;
}
