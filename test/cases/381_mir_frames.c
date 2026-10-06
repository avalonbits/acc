/* Frames opt-acc's machine-level backend lays out: spills whose values are
 * never kept at once in one slot -- three runs of six values held across
 * calls, in a frame of six bytes -- and a function of 45 locals, all of
 * them values and none given room, with an inlined body's array in reach.
 * And a _Bool made of anything passed as an argument; a call's answer let
 * go with (void); a _Bool predicate read in place in a loop's test,
 * branched on as the flags it is made from. */
static int seen;

static int step(int v) { seen++; return v * 3 + 1; }
static int mix(int a, int b, int c, int d, int e, int x)
{
    return a + 2 * b + 3 * c + 4 * d + 5 * e + 6 * x;
}

static int runs(int k)
{
    int a = step(k), b = step(a), c = step(b), d = step(c), e = step(d), x = step(e);
    int s = mix(a, b, c, d, e, x);

    {
        int p = step(1), q = step(p), r = step(q), t = step(r), u = step(t), w = step(u);

        s += mix(p, q, r, t, u, w);
    }
    {
        int p = step(2), q = step(p), r = step(q), t = step(r), u = step(t), w = step(u);

        s -= mix(p, q, r, t, u, w);
    }
    return s;
}

__attribute__((always_inline)) static inline int pair(int v)
{
    int t[2];

    t[0] = v;
    t[1] = step(v);
    return t[0] * t[1];
}

static int many(int k)
{
    int v1 = k + 1, v2 = k + 2, v3 = k + 3, v4 = k + 4, v5 = k + 5, v6 = k + 6;
    int v7 = k + 7, v8 = k + 8, v9 = k + 9, v10 = k + 10, v11 = k + 11, v12 = k + 12;
    int v13 = k + 13, v14 = k + 14, v15 = k + 15, v16 = k + 16, v17 = k + 17, v18 = k + 18;
    int v19 = k + 19, v20 = k + 20, v21 = k + 21, v22 = k + 22, v23 = k + 23, v24 = k + 24;
    int v25 = k + 25, v26 = k + 26, v27 = k + 27, v28 = k + 28, v29 = k + 29, v30 = k + 30;
    int v31 = k + 31, v32 = k + 32, v33 = k + 33, v34 = k + 34, v35 = k + 35, v36 = k + 36;
    int v37 = k + 37, v38 = k + 38, v39 = k + 39, v40 = k + 40, v41 = k + 41, v42 = k + 42;
    int v43 = k + 43, v44 = k + 44, v45 = k + 45;

    return v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 + v12 + v13 + v14
           + v15 + v16 + v17 + v18 + v19 + v20 + v21 + v22 + v23 + v24 + v25 + v26
           + v27 + v28 + v29 + v30 + v31 + v32 + v33 + v34 + v35 + v36 + v37 + v38
           + v39 + v40 + v41 + v42 + v43 + v44 + v45 + pair(k);
}

static int truth(_Bool b, int w) { return b ? w : 0; }
static int as_int(_Bool b) { return b; }

static int truths(int a, const char *p)
{
    return truth(a, 1) + truth(p, 2) + truth(a & 4, 4) + truth(0, 8) + truth(3, 16);
}

/* The _Bool itself, 0 or 1 whatever made it: as_int answers it as it is. */
static int ones(int a, const char *p)
{
    return as_int(a & 4) + as_int(p) * 10 + as_int(a) * 100 + as_int(256) * 1000;
}

static int dropped(int a)
{
    (void) step(a);
    (void) a;
    return a + 1;
}

static const unsigned char kinds[128] = { [' '] = 1, ['\t'] = 1, ['x'] = 2 };

__attribute__((always_inline)) static inline _Bool blank(char c)
{
    return (kinds[(unsigned char) c & 0x7f] & 1) != 0;
}

static int skip(const char *p, const char *e)
{
    const char *at = p;

    while (at < e && blank(*at))
        at++;
    return (int) (at - p);
}

int main(void)
{
    static const char text[] = "  \t x  ";
    int ok = 0;

    ok += runs(1) == 2997;
    ok += many(0) == 45 * 46 / 2;
    ok += many(2) == 45 * 46 / 2 + 45 * 2 + 2 * 7;
    ok += truths(5, "") == 1 + 2 + 4 + 16;
    ok += truths(0, 0) == 16;
    ok += ones(5, "") == 1111 && ones(0, 0) == 1000;
    ok += dropped(4) == 5 && seen == 18 + 2 + 1;
    ok += skip(text, text + 7) == 4 && skip(text + 4, text + 7) == 0;

    return ok * 5 + 2;
}
