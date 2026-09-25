/* `&&` and `||` straight into a branch jump from each side rather than
 * making a one or a zero to test: in if, while, for, do-while and ?:,
 * nested, with signed comparisons -- two jumps each -- and with side
 * effects on the right that have to happen only when the left leaves the
 * answer open. And the same operators as values, where the one or zero
 * stays. */
static int calls;

static int t(int v)
{
    calls = calls * 3 + v + 1;
    return v;
}

static unsigned long mix(unsigned long h, long x)
{
    return (h * 31 + (unsigned long) x) & 0xffffffffUL;
}

static int vals[] = { -3, 0, 1, 2, -1, 7 };

int main(void)
{
    unsigned long h = 0;
    int i, j;

    for (i = 0; i < 6; i++)
        for (j = 0; j < 6; j++) {
            int a = vals[i], b = vals[j], n;

            calls = 0;
            if (t(a) && t(b)) h = mix(h, 1);
            if (t(a) || t(b)) h = mix(h, 2);
            if (a < b && b < 5) h = mix(h, 3);
            if (a < b || a > 5) h = mix(h, 4);
            if ((a < 0 && b < 0) || (a > 0 && b > 0)) h = mix(h, 5);
            if (!(a && b) || (a >= b && t(a - b))) h = mix(h, 6);
            if (a && (b || t(a))) h = mix(h, 7);
            h = mix(h, a && b);
            h = mix(h, a || t(b));
            h = mix(h, (a < b && b) ? 8 : 9);
            n = 0;
            while (n < 4 && (a + n < 3 || b > n))
                n++;
            h = mix(h, n);
            for (n = 0; n < 5 && !(a == n || b == n); n++)
                ;
            h = mix(h, n);
            n = 0;
            do
                n++;
            while (n < 3 && (a > n || t(b)));
            h = mix(h, n);
            h = mix(h, calls);
        }

    /* From the same program built by gcc on the host: every value is
     * small, so the width of int does not come into it. */
    return h == 211996574UL ? 42 : 1;
}
