/* A constant a function returns more than once, which acc compiles as a
 * jump back to the first return of it: from ifs, loops, a switch and nested
 * blocks, in functions returning a _Bool, a char, an unsigned char, an int
 * and a pointer, after comparisons whose code was taken back and written
 * again, and with the same constants in the next function, whose returns
 * are its own. */
static unsigned long h = 1;

static void mix(long v)
{
    h = (h * 31 + (unsigned long) v) & 0xffffffffUL;
}

static _Bool is_small(int x)
{
    if (x < -5)
        return 0;
    if (x > 5)
        return 0;
    if (x == 0)
        return 1;
    if (x == 3 || x == -3)
        return 1;
    return x & 1 ? 1 : 0;
}

static char grade(int s)
{
    switch (s / 10) {
    case 10:
    case 9: return 'A';
    case 8: return 'B';
    case 7: if (s % 10 > 5) return 'B'; return 'C';
    case 6: return 'C';
    default:
        if (s < 0)
            return '?';
        return 'F';
    }
}

static unsigned char classify(unsigned char c)
{
    if (c >= '0' && c <= '9')
        return 200;
    if (c >= 'a' && c <= 'z')
        return 7;
    if (c >= 'A' && c <= 'Z')
        return 7;
    if (c == ' ')
        return 200;
    return 255;
}

static int search(const int *a, int n, int want)
{
    int i;

    if (!a)
        return -1;
    for (i = 0; i < n; i++) {
        if (a[i] == want)
            return i;
        if (a[i] > want)
            return -1;
    }
    {
        int again = n - 1;

        while (again > 0) {
            if (a[again] == -want)
                return -2;
            again--;
        }
    }
    return -1;
}

static const char *name(int k)
{
    if (k < 0)
        return 0;
    if (k == 1)
        return "one";
    if (k > 100)
        return 0;
    if (k == 2)
        return "two";
    return 0;
}

static int zeroes(int x)
{
    if (x == 1) return 0;
    if (x == 2) return 1;
    if (x == 5) return 0;
    if (x > 9) return 1;
    return 0;
}

int main(void)
{
    static const int sorted[6] = { -9, -4, 1, 3, 8, 12 };
    int i;

    for (i = -8; i <= 8; i++)
        mix(is_small(i));
    for (i = -5; i <= 105; i += 7)
        mix(grade(i));
    for (i = 0; i < 128; i += 3)
        mix(classify((unsigned char) i));
    mix(search(sorted, 6, 3)); mix(search(sorted, 6, 4)); mix(search(sorted, 6, 9));
    mix(search(sorted, 6, 99)); mix(search(0, 6, 1));
    for (i = -1; i <= 3; i++) {
        const char *s = name(i);

        mix(s ? s[1] : 0);
    }
    mix(name(200) == 0);
    for (i = 0; i < 12; i++)
        mix(zeroes(i));

    /* From gcc on the host. */
    return h == 1313597254UL ? 42 : 1;
}
