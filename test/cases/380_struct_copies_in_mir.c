/* `*p = s` for a struct, in functions opt-acc's machine-level backend now
 * makes: its bytes copied with ldir -- from a global's, from another
 * pointer's, from a parameter's, into a local in the frame, a member struct
 * into another's, one after another in a loop -- and what is written through
 * the pointer read back after. A struct assigned twice over, `*p = *q = *r`,
 * is left to the other backends, and comes to the same. */
struct d { int a, b; char c; };
struct o { char k; struct d in; };

static const struct d none = { 1, 2, 3 };

static void reset(struct d *p) { *p = none; }
static int copied(struct d *p, const struct d *q) { *p = *q; return p->a + p->c; }
static void from_param(struct d *p, struct d s) { *p = s; }
static void inner(struct o *p, const struct o *q) { p->in = q->in; }
static void each(struct d *p, const struct d *q, int n) { while (n--) *p++ = *q++; }
static void twice(struct d *p, struct d *q, const struct d *r) { *p = *q = *r; }

static int local(const struct d *q)
{
    struct d s;

    s = *q;
    s.b += 10;
    return s.a + s.b + s.c;
}

int main(void)
{
    struct d x = { 0, 0, 0 }, y = { 7, 8, 9 }, many[3] = { { 1, 1, 1 }, { 2, 2, 2 }, { 3, 3, 3 } };
    struct d to[3];
    struct o oa = { 'a', { 0, 0, 0 } }, ob = { 'b', { 4, 5, 6 } };
    int ok = 0;

    reset(&x);
    ok += x.a == 1 && x.b == 2 && x.c == 3;
    ok += copied(&x, &y) == 16 && x.b == 8;
    inner(&oa, &ob);
    ok += oa.k == 'a' && oa.in.a == 4 && oa.in.b == 5 && oa.in.c == 6;
    each(to, many, 3);
    ok += to[0].a == 1 && to[1].b == 2 && to[2].c == 3;
    ok += local(&y) == 7 + 18 + 9 && y.b == 8;
    twice(&x, &to[0], &ob.in);
    ok += x.a == 4 && to[0].c == 6;
    from_param(&x, y);
    ok += x.a == 7 && x.b == 8 && x.c == 9;

    return ok * 6;
}
