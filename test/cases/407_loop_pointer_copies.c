/* A loop's pointer -- written as the loop starts and again on its way
 * round -- read through for its members, which opt-acc's machine-level
 * backend copies into IY for each read and may now keep the pointer in
 * IY itself: stepped by one or by two on different paths, passed to a
 * call, read again after the step, and in a loop inside another. */
typedef struct {
    unsigned char tag, a, b;
} item;

static int seen;

static __attribute__((noinline)) void note(const item *p)
{
    seen += p->tag;
}

static int walk(const item *p, int n)
{
    int t = 0;

    while (n-- > 0) {
        if (p->a & 1) {
            t += p->b;
            p++;
            t += p->tag;        /* the next one's, read after the step */
        } else {
            note(p);
            t -= p->tag;
            p += 2;
            n--;
        }
    }

    return t;
}

static int rows(const item *p, int nrows, int ncols)
{
    int t = 0, r, c;

    for (r = 0; r < nrows; r++) {
        const item *q = p + r * ncols;

        for (c = 0; c < ncols; c++, q++)
            t += q->a * (c + 1) - q->b;
    }

    return t;
}

int main(void)
{
    static const item list[] = {
        { 1, 1, 10 }, { 2, 0, 20 }, { 3, 3, 30 }, { 4, 0, 40 },
        { 5, 5, 50 }, { 6, 1, 60 }, { 7, 2, 70 }, { 8, 1, 80 },
    };
    int ok = 0;

    /* +10 +2, to item 2; -2 (noted), to 4; -4 (noted), to 6; +60 +7,
     * to 7; -7 (noted), and done: 10 + 2 - 2 - 4 + 60 + 7 - 7. */
    ok += walk(list, 7) == 66 && seen == 2 + 4 + 7;
    ok += rows(list, 2, 4) == (1 * 1 + 0 * 2 + 3 * 3 + 0 * 4 - 100)
                              + (5 * 1 + 1 * 2 + 2 * 3 + 1 * 4 - 260);

    return ok == 2 ? 42 : ok;
}
