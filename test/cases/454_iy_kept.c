/* p->a, p->b, ... with the pointer in IY loaded once: kept across code that
 * leaves IY and the pointer's slot alone, and loaded again after anything
 * that may not -- the pointer stored to, a call, a jump in, another
 * pointer's member, or a function that takes a local's address. */
struct rec {
    int a, b;                   /* 0, 3: a is read through p itself */
    char c, d;                  /* 6, 7 */
    int e;                      /* 8 */
    struct rec *next;           /* 11 */
};

static struct rec r1, r2;
static int calls;

static int bump(int x)
{
    calls++;
    return x + 1;
}

static void fill(struct rec *p, int v)
{
    p->a = v;
    p->b = v + 1;
    p->c = 3;
    p->d = (char) p->c;
    p->e = p->a + p->b;
}

static int moved(struct rec *p, struct rec *q)
{
    int s, n;

    p->e = 10;
    p = q;                      /* the slot stored to: loaded again */
    n = 5;                      /* HL something else, and nothing but stores */
    p->b = 20 + n - 5;
    s = p->b;
    p->e = bump(p->b);          /* a call between */
    return s + p->e;
}

static int other(struct rec *q)
{
    return q->e;                /* IY loaded with q, the caller's p lost */
}

static int called(struct rec *p, struct rec *q)
{
    int n;

    p->e = 3;
    n = other(q);               /* a call: IY loaded again after */
    n = n + p->b;
    return n;
}

static int reread(struct rec *p, struct rec *q)
{
    int n;

    p->e = 10;
    p = q;                      /* the slot stored to */
    n = 5;                      /* and HL something else: p loaded again */
    n = p->b + n;               /* read: IY loaded again */
    return n;
}

static int rejumped(struct rec *p, struct rec *q)
{
    int k = 0, m, x, t = 0;

    p->e = 1;
again:
    m = 7;                      /* a label before: arrived at with q */
    x = p->b;
    t = t + x + m;
    if (k++ == 0) {
        p = q;
        goto again;
    }
    return t;
}

static int jumped(struct rec *p, struct rec *q)
{
    int n = 0, m;

    p->e = 1;
again:
    m = 7;                      /* a label between: arrived at with q */
    p->b = n + m - 6;
    if (n++ == 0) {
        p = q;
        goto again;
    }
    return r1.b * 10 + r2.b;
}

static int neighbour(struct rec *p)
{
    int before = 5, after;
    struct rec *q = p;

    p->e = 7;
    before = 9;                 /* a store beside the slot */
    after = q->e;
    p->b = before;
    return after + p->b;
}

static int addressed(struct rec *p, struct rec *q)
{
    struct rec **pp = &p;

    p->e = 1;
    *pp = q;                    /* p changed through its address */
    p->b = 2;
    return r1.b * 10 + r2.b;
}

int main(void)
{
    r1.next = &r2;
    fill(&r1, 5);
    if (r1.a != 5 || r1.b != 6 || r1.c != 3 || r1.d != 3 || r1.e != 11)
        return 1;
    if (moved(&r1, &r2) != 20 + 21 || r1.e != 10 || r2.b != 20 || calls != 1)
        return 2;
    r1.b = r2.b = 0;
    if (jumped(&r1, &r2) != 12)
        return 3;
    if (neighbour(&r1) != 7 + 9 || r1.b != 9)
        return 4;
    r1.b = 1;
    r2.b = 2;
    if (reread(&r1, &r2) != 7 || rejumped(&r1, &r2) != 8 + 9)
        return 6;
    r2.e = 40;
    if (called(&r1, &r2) != 40 + 1)
        return 7;
    r1.b = r2.b = 0;
    if (addressed(&r1, &r2) != 2)
        return 5;

    return 42;
}
