/* Static functions called from one place, which opt-acc's OPTACC_INLINE
 * reads in place of the call: returns from loops and from nested ifs, a
 * void body, a body inside another, arguments converted to their
 * parameters' types, a local named as the caller's, a call in a loop, and
 * bodies whose locals stay in memory -- the next body taking their room,
 * at another type -- beside values that live through the whole caller. */
void touch(int *a, int *b)
{
    *a = 5;
    *b = 7;
}

static int first_zero(const char *p, int n)
{
    for (int i = 0; i < n; i++)
        if (!p[i])
            return i;

    return -1;
}

static int classify(int c)
{
    if (c < 0) {
        if (c < -100)
            return 1;
        return 2;
    }
    if (c == 0)
        return 3;

    return 4;
}

static void fill(char *p, int n, char v)
{
    while (n--)
        *p++ = v;
}

static int inner(int a)
{
    return a * 3;
}

static int outer(int a)
{
    int b = inner(a + 1);

    return b - a;
}

static int narrowed(unsigned char c, short s)
{
    return c + s;
}

static int shadow(int total)
{
    int right = total * 2;

    return right + 1;
}

static int step(int x)
{
    return x + 2;
}

static int in_memory(int k)
{
    int x, y;

    touch(&x, &y);

    return x * y + k;
}

static char *other_type(char *p, int n)
{
    char *hit;

    if (n)
        hit = p + n;
    if (n)
        return hit;

    return p;
}

/* Values held across both bodies, with more of them than registers. */
static int held(int a, int b, int c)
{
    int d = a + b, e = b + c, f = a + c, g = a * 2, h = b * 2;
    int s = in_memory(a);
    char buf[4] = "xyz";
    char *t = other_type(buf, 2);

    return d + e + f + g + h + s + *t;
}

int main(void)
{
    char text[6] = { 1, 2, 0, 4, 5, 6 };
    char area[4];
    int right = 0, total = 0, got;

    /* Each call the start of its statement: under `right +=` it would have
     * a value on the stack beneath it, and be an ordinary call. */
    got = first_zero(text, 6);
    right += got == 2;
    got = classify(-5);
    right += got == 2;
    fill(area, 4, 9);
    right += area[0] == 9 && area[3] == 9;
    got = outer(4);
    right += got == 11;
    got = narrowed(300, 70000);
    right += got == 44 + 4464;
    got = shadow(5);
    right += got == 11 && right == 5;
    for (int i = 0; i < 3; i++)
        total = step(total);
    right += total == 6;
    /* 3 + 5 + 4 + 2 + 4 + (35 + 1) + 'z' */
    got = held(1, 2, 3);
    right += got == 54 + 'z';

    return right == 8 ? 42 : right;
}
