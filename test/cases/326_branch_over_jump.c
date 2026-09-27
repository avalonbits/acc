/* A condition that guards only a jump -- continue, break, a return of a
 * constant already returned, goto -- which acc compiles as one jump the
 * other way: on zero and not, carry and not, and the sign and overflow
 * jumps a signed long comparison takes, and a label on the jump that was
 * jumped over, which keeps it. */
static unsigned long h = 1;

static void mix(unsigned long v)
{
    h = ((h ^ (h >> 13)) * 31 + v) & 0xffffffffUL;
}

static int first_zero(const unsigned char *p, int n)
{
    int i;

    for (i = 0; i < n; i++)
        if (!p[i])
            return i;

    return -1;
}

static int classify(int x)
{
    if (x == 0)
        return 0;
    if (x < 10)
        return 1;
    if (x > 100)
        return 0;
    if ((unsigned) x > 50u)
        return 1;

    return 2;
}

static long walk(const long *v, int n)
{
    long total = 0;
    int i;

    for (i = 0; i < n; i++) {
        if (v[i] < -1000L)
            continue;
        if (v[i] > 1000000L)
            break;
        if (v[i] == 7L)
            continue;
        total += v[i];
    }

    return total;
}

static int with_label(int x)
{
    int n = 0;

again:
    if (x > 3)
        goto done;
    n++;
    x++;
    if (x != 2)
        goto again;
    n += 10;
done:
    return n;
}

/* A label on the jump the condition would jump over: something else jumps
 * to it, so it has to stay. */
static int label_on_jump(int x)
{
    int n = 0;

    if (x == 1)
        goto inner;
    if (x > 3) {
inner:
        goto done;
    }
    n = 5;
done:
    return n + x;
}

int main(void)
{
    static const unsigned char bytes[] = { 3, 9, 0, 4, 0 };
    static const long v[] = { 5L, -2000L, 7L, 70000L, -3L, 2000000L, 11L };
    int i;

    mix((unsigned long) first_zero(bytes, 5));
    mix((unsigned long) first_zero(bytes, 2));
    for (i = -5; i < 120; i += 7)
        mix((unsigned long) classify(i));
    mix((unsigned long) walk(v, 7));
    mix((unsigned long) walk(v, 3));
    for (i = -2; i < 6; i++) {
        mix((unsigned long) with_label(i));
        mix((unsigned long) label_on_jump(i));
    }

    /* From gcc on the host, with long taken as 32 bits. */
    return h == 2426641930UL ? 42 : 1;
}
