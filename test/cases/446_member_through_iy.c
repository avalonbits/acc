/* p->m read through IY: the pointer loaded from its slot and the member
 * read at (iy+n), where acc had added the offset in HL. Members at each
 * reach -- one to three bytes in, which acc steps to with inc hl, further
 * in with an add, the last byte IY can reach and the first it cannot --
 * read as ints, as signed and unsigned chars, and through a pointer read
 * the same way. A register local holds IY in one function, which leaves
 * the reads as they were: p->edge read through IY there would lose q. */
struct rec {
    unsigned char tag;          /* 0 */
    int a;                      /* 1 */
    int b;                      /* 4 */
    signed char sc;             /* 7 */
    char pad[116];              /* 8 */
    int last;                   /* 124: reaches 126 */
    unsigned char edge;         /* 127 */
    int beyond;                 /* 128 */
    struct rec *next;           /* 131 */
};

static struct rec r1, r2;

static int sum(const struct rec *p)
{
    return p->tag + p->a + p->b + p->sc + p->last + p->edge + p->beyond;
}

static int chain(const struct rec *p)
{
    int n = 0;

    while (p) {
        n += p->b;
        p = p->next;
    }

    return n;
}

static int held(const struct rec *p)
{
    register const struct rec *q = p->next;

    return p->edge + q->a + p->a;
}

int main(void)
{
    r1.tag = 200;
    r1.a = 1000;
    r1.b = -7;
    r1.sc = -100;
    r1.last = 70000;
    r1.edge = 255;
    r1.beyond = 3;
    r1.next = &r2;
    r2.b = 50;
    r2.a = 9;

    if (sum(&r1) != 200 + 1000 - 7 - 100 + 70000 + 255 + 3)
        return 1;
    if (chain(&r1) != 43)
        return 2;
    if (held(&r1) != 1000 + 9 + 255)
        return 3;

    return 42;
}
