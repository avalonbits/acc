/* p->m = v through IY: the pointer loaded from its slot into IY and the
 * member written at (iy+n) -- a byte number with ld (iy+n), n, a byte
 * local through A, an int number or local through HL -- where acc had
 * added the offset in HL first. Members at one to three bytes, which acc
 * steps to with inc hl, further in with an add, the last byte IY reaches
 * and the first it cannot. The assignment's value used after; the pointer
 * changed between two stores; a register local holding IY, which leaves
 * them as they were. */
struct rec {
    unsigned char tag;          /* 0 */
    int a;                      /* 1 */
    int b;                      /* 4 */
    signed char sc;             /* 7 */
    char pad[116];              /* 8 */
    int last;                   /* 124 */
    unsigned char edge;         /* 127 */
    int beyond;                 /* 128 */
    struct rec *next;           /* 131 */
};

static struct rec r1, r2;

static void fill(struct rec *p, int v, signed char c, unsigned char u)
{
    p->tag = 200;
    p->a = 1000;
    p->b = v;
    p->sc = c;
    p->last = 70000;
    p->edge = u;
    p->beyond = 3;
}

static int chain(struct rec *p, int v)
{
    int r = (p->b = v) + 1;

    p = p->next;
    p->b = v * 2;
    p->sc = -3;

    return r;
}

static int held(struct rec *p)
{
    register struct rec *q = p->next;

    p->edge = 9;
    p->b = 11;

    return q->a;
}

int main(void)
{
    r1.next = &r2;
    r2.a = 77;
    fill(&r1, -7, -100, 255);
    if (r1.tag != 200 || r1.a != 1000 || r1.b != -7 || r1.sc != -100
        || r1.last != 70000 || r1.edge != 255 || r1.beyond != 3)
        return 1;
    if (chain(&r1, 21) != 22 || r1.b != 21 || r2.b != 42 || r2.sc != -3)
        return 2;
    if (held(&r1) != 77 || r1.edge != 9 || r1.b != 11)
        return 3;

    return 42;
}
