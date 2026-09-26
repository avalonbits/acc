/* Returns from everywhere a function can return -- nested ifs, loops, a
 * switch, the last statement, falling off the end of a void function --
 * of every kind of value: a char in A, an int, a long, a struct. acc sends
 * each to one epilogue at the end of the function, and takes back the jump
 * of a return that is the last thing in it. */
struct pair { int a; long b; char c; };

static unsigned long h = 1;

static void mix(long v)
{
    h = (h * 31 + (unsigned long) v) & 0xffffffffUL;
}

static char sign(int x)
{
    if (x < 0)
        return -1;
    if (x > 0)
        return 1;
    return 0;
}

static int find(const int *a, int n, int want)
{
    int i;

    for (i = 0; i < n; i++)
        if (a[i] == want)
            return i;

    return -1;
}

static long pick(int k)
{
    switch (k) {
    case 0: return 100000L;
    case 1: return -5L;
    case 2: { long x = k * 70000L; if (x > 100000L) return x; } break;
    default: return k;
    }
    return 7L;
}

static struct pair make(int k)
{
    struct pair p = { k, k * 100000L, 'a' };

    if (k & 1) {
        p.c = 'o';
        return p;
    }
    while (k > 10) {
        p.a = k;
        if (k % 7 == 0)
            return p;
        k--;
    }
    p.b = -1;
    return p;
}

static int count;

static void bump(int k)
{
    if (k < 0)
        return;
    count += k;
    if (k > 100)
        return;
    count++;
}

static int nested(int a, int b)
{
    if (a) {
        if (b)
            return a + b;
        else
            return a - b;
    } else {
        do {
            if (b > 3)
                return b;
            b++;
        } while (b < 10);
    }
    return -a;
}

int main(void)
{
    static const int a[5] = { 4, 9, 2, 7, 9 };
    int i;

    for (i = -3; i <= 3; i++)
        mix(sign(i));
    mix(find(a, 5, 7)); mix(find(a, 5, 9)); mix(find(a, 5, 1));
    for (i = 0; i < 5; i++)
        mix(pick(i));
    for (i = 0; i < 16; i += 3) {
        struct pair p = make(i + 5);

        mix(p.a); mix(p.b); mix(p.c);
    }
    bump(-1); bump(5); bump(200);
    mix(count);
    mix(nested(2, 3)); mix(nested(2, 0)); mix(nested(0, 1)); mix(nested(0, 5));

    /* From gcc on the host. */
    return h == 654605038UL ? 42 : 1;
}
