/* A function said always_inline, read in place at every call however many
 * there are -- in a loop, with two returns and a loop of its own -- and
 * one said noinline, called though it could be read in place. */
static __attribute__((always_inline)) int clamp_sum(const int *p, int n, int hi)
{
    int s = 0;

    while (n-- > 0)
        s += *p++;
    if (s > hi)
        return hi;

    return s;
}

static __attribute__((noinline)) int twice(int x)
{
    return x * 2;
}

int main(void)
{
    static const int v[5] = { 3, 9, 4, 8, 6 };
    int i, total = 0, a, b;

    a = clamp_sum(v, 5, 100);
    b = clamp_sum(v, 5, 20);
    for (i = 1; i <= 5; i++)
        total += clamp_sum(v, i, 15);

    return a == 30 && b == 20 && total == 3 + 12 + 15 + 15 + 15 && twice(21) == 42 ? 42 : 1;
}
