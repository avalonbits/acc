/* Functions declared always_inline -- bodies with locals, loops, several
 * returns, a goto and its label, an #if decided where the body was
 * written, a parameter written through its address, locals named as the
 * caller's are, a global the caller shadows, one called in another's
 * argument, void ones, answers converted to the function's type -- called
 * at a statement and in the middle of an expression. Against gcc on the
 * host. Written for compiling such calls in place, which was measured and
 * dropped; it holds for any way of compiling them. */
#define MODE 2

static unsigned long h;

static void mix(long x)
{
    h = (h * 31 + (unsigned long) x) & 0xffffffffUL;
}

__attribute__((always_inline)) static inline int skip_spaces(const char **pp,
                                                             const char *e)
{
    const char *p = *pp;
    int n = 0;

    while (p < e && (*p == ' ' || *p == '\t')) {
        p++;
        n++;
    }
    *pp = p;
    if (p == e)
        return -1;

    return n;
}

__attribute__((always_inline)) static inline unsigned char classify(char c)
{
    int k;

#if MODE == 2
    k = 2;
#else
    k = 5;
#endif
    if (c >= '0' && c <= '9')
        return 1;
    if (c == '(')
        goto open;
    if (c >= 'a' && c <= 'z')
        return k;
    return 300;                         /* 44 as an unsigned char */
open:
    return 9;
}

__attribute__((always_inline)) static inline int sum_to(int n)
{
    int t = 0, i;

    for (i = 1; i <= n; i++) {
        if (i == 7)
            continue;
        t += i;
    }

    return t;
}

__attribute__((always_inline)) static inline int both(const char *s)
{
    int p = 0;                          /* named as the caller's pointer */

    while (*s)
        p += classify(*s++);

    return p;
}

__attribute__((always_inline)) static inline void bump(int *x, int by)
{
    if (by < 0)
        return;
    *x += by;
}

static int count = 100;

__attribute__((always_inline)) static inline int add_count(int x)
{
    int y = x + count;                  /* the file's count, whatever the */
                                        /* caller has by that name */
    count++;
    return y;
}

/* Two returns each, one called in the other's first argument: each has
 * its own returns to see to. */
__attribute__((always_inline)) static inline int clamp(int v, int hi)
{
    if (v > hi)
        return hi;
    if (v < 0)
        return 0;

    return v;
}

static inline int fact(int n)            /* gcc refuses it always_inline */
{
    if (n <= 1)
        return 1;

    return n * fact(n - 1);
}

/* Defined again after the bodies: what they said was decided then. */
#undef MODE
#define MODE 5

int main(void)
{
    static const char text[] = "  ab(9 z\t x";
    const char *p = text, *e = text + sizeof text - 1;
    int n, k = 0;

    n = skip_spaces(&p, e);
    mix(n);
    mix(p - text);
    mix(classify(*p));
    p += 2;
    mix(classify(*p));
    mix(classify('9'));
    mix(classify('#'));
    mix(both(text));
    mix(sum_to(10));
    bump(&k, 5);
    bump(&k, -3);
    mix(k);
    if (skip_spaces(&p, e) >= 0)
        mix(*p);
    mix(1 + sum_to(4) * 2);             /* not where a body can go */
    mix(fact(6));
    while (skip_spaces(&p, e) >= 0) {
        mix(*p);
        p++;
    }
    mix(p - text);
    {
        int count = 7;                  /* the caller's own */

        mix(add_count(count));
        mix(add_count(1) + count);
    }
    mix(count);
    mix(classify('q'));                 /* MODE was 2 where it was written */
    n = clamp(clamp(k * 3, 10) + 4, 12);
    mix(n);
    n = clamp(clamp(-5, 10) - 1, 12);
    mix(n);

    /* From gcc on the host. */
    return h == 76488156UL ? 42 : 1;
}
