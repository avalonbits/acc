/* Local pointers walked -- read through, stepped, compared -- with
 * everything that could make a copy of one kept anywhere but its slot go
 * stale in between: a call, a store to its slot, a store through its
 * address, a jump into the middle, a block move, reads that use IY,
 * pointers to shorts, ints, longs and structs stepped. Against gcc on
 * the host. Written for a pointer held in IY, which was dropped; what it
 * checks is what any such scheme has to get right. */
static const char text[] = "  one two\tthree  four five";
static const short shorts[] = { 1, -2, 300, -400, 5000, 6, 0 };
static const int ints[] = { 10, 20, 30, 40, 50, 0 };

struct holder { const char *p; int n; };
struct big { const char *p; char pad[24]; };
struct mark { int off; unsigned char k; int len; };

static const struct mark marks[] = { { 1, 2, 3 }, { 40, 5, -6 }, { 700, 8, 9 },
                                     { 0, 0, 0 } };
static const long longs[] = { 70000L, -2L, 123456789L, 0L };

/* A pointer to a struct steps by the struct's size and not by the three
 * bytes its type is; a pointer to a long, by four. */
static long walk_marks(void)
{
    const struct mark *mk = marks;
    const long *lp = longs;
    long t = 0;
    int u = 0;

    /* Nothing in the loop calls. */
    while (mk->off || mk->k) {
        u = u + mk->off + mk->k + mk->len;
        mk++;
    }
    mk--;
    u += mk->off;
    --mk;
    u += (++mk)->len;
    t = u;
    while (*lp)
        t += *lp++ % 1000;

    return t;
}

static unsigned long h;

static void mix(long x)
{
    h = (h * 31 + (unsigned long) x) & 0xffffffffUL;
}

static void skip_one(const char **pp) { (*pp)++; }

/* Reads a short through its parameter, which uses IY. */
static int first_short(const short *sp) { return *sp + 1; }

/* Each read, then one thing that changes what is read, then the read
 * again with nothing else between. x is 0, so the else is the way taken. */
static int tight(int x)
{
    static char buf[] = "abcdefgh";
    struct big a, b;
    const char *p = buf, **pp = &p, *q = buf + 7;
    int c1, c2, c3, c4, c5, c6, c7, c8, c9, c10;

    c1 = *p;
    c2 = first_short(shorts + 2);       /* a call, which writes IY */
    c3 = *p;
    p = buf + 3;                        /* a store to its slot */
    c4 = *p;
    *pp = buf + 5;                      /* a store through its address */
    c5 = *p;
    if (x)
        c6 = *q;                        /* on the way not taken */
    else
        c6 = 9;
    c7 = *q;                            /* after the join */
    a.p = buf + 1;
    b.p = buf + 6;
    c8 = *a.p;
    a = b;                              /* a block move over it: ldir */
    c9 = *a.p;
    c10 = *p;

    return c1 + 2 * c2 + 3 * c3 + 5 * c4 + 7 * c5 + 11 * c6 + 13 * c7
           + 17 * c8 + 19 * c9 + 23 * c10;
}
static int touch(int x) { return x + 1; }

static int count_words(const char *p)
{
    int n = 0;

    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (!*p)
            break;
        n++;
        while (*p && *p != ' ' && *p != '\t')
            p++;
    }

    return n;
}

int main(void)
{
    const char *p = text, *q;
    const short *s = shorts;
    const int *w = ints;
    struct holder a, b;
    long sum = 0;
    int i;

    mix(count_words(text));
    mix(tight(0));
    mix(walk_marks());

    /* Stepped, read, stepped again, with a call in the middle. */
    while (*p == ' ')
        p++;
    mix(*p);
    mix(touch(*p++));
    mix(*p++);
    mix(*++p);
    p += 2;
    mix(*p);
    p--;
    mix(*p);

    /* A callee steps it through its address; then a store through a
     * pointer to it. */
    q = p;
    skip_one(&q);
    mix(*q);
    mix(*q++);
    {
        const char **pq = &q;

        *pq = text + 3;
        mix(*q);
        mix(q[1]);
    }

    /* Shorts, whose reading uses IY itself, and ints. */
    while (*s) {
        sum += *s;
        mix(*s++ & 0x7ff);
    }
    while (*w)
        sum += *w++ * 3;
    mix(sum);

    /* A struct holding the pointer, copied over it. */
    a.p = text + 5;
    a.n = 1;
    b.p = text;
    b.n = 2;
    mix(*a.p);
    a = b;
    mix(*a.p + a.n);

    /* Jumped into, and a switch. */
    p = text;
    i = 0;
    goto middle;
    while (i < 6) {
        p++;
middle:
        switch (*p) {
        case ' ': mix(1); break;
        case 'o': mix(2); p++; break;
        default:  mix(*p);
        }
        i++;
    }
    mix(*p);

    /* Nested walks with a long in between. */
    for (p = text; *p; p++) {
        long big = (long) *p * 70000L;

        for (q = p; *q == *p; q++)
            big += *q;
        mix(big % 1000);
    }

    /* From gcc on the host. */
    return h == 3362965280UL ? 42 : 1;
}
