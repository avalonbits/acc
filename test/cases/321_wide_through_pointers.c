/* Wide values read and written through pointers, which acc moves three
 * bytes at a time through IY: longs, long longs and floats, stored from a
 * variable's own slot and from a value worked out, the value an assignment
 * through a pointer comes to used again, shifts by whole bytes read from a
 * variable where it is, and an operator whose right operand is read in
 * place with the answer where its left one was. And the same where so
 * many temporaries are alive that the scratch is out of reach. */
static unsigned long h = 1;

static void mix(unsigned long v)
{
    h = ((h ^ (h >> 13)) * 31 + v) & 0xffffffffUL;
}

static void mixll(unsigned long long v)
{
    mix((unsigned long) v);
    mix((unsigned long) (v >> 32));
}

static long lcell[3];
static unsigned long long qcell[2];
static float fcell[2];

static void through(long *lp, unsigned long long *qp, float *fp, long v,
                    unsigned long long q)
{
    long got, again;
    unsigned long long qgot;

    *lp = v;
    lp[1] = v + 3;
    got = *lp;
    again = lp[1] = got ^ 0x0f0f0f0fL;
    mix((unsigned long) got); mix((unsigned long) again); mix((unsigned long) lp[1]);
    *qp = q;
    qp[1] = *qp + 7;
    qgot = qp[1];
    mixll(qgot);
    *fp = 2.5f;
    fp[1] = *fp * 3.0f;
    mix((unsigned long) (long) fp[1]);
    mix((unsigned long) (v >> 8)); mix((unsigned long) (v >> 16));
    mix((unsigned long) (v >> 24)); mix((unsigned long) v << 8);
    mix((unsigned long) v << 16); mix((unsigned long) v << 24);
    mix((unsigned long) ((v >> 8) ^ (*lp & 0xffL)));
}

static void deep(long *lp, long v)
{
    long x1 = v, x2 = v + 1, x3 = v + 2, x4 = v + 3, x5 = v + 4, x6 = v + 5,
         x7 = v + 6, x8 = v + 7, x9 = v + 8, x10 = v + 9;

    mix((unsigned long) ((x1 ^ x2) + ((x3 ^ x4) + ((x5 ^ x6) + ((x7 ^ x8)
        + ((x9 ^ x10) + ((x1 ^ x3) + ((x2 ^ x4) + ((x5 ^ x7)
        + (*lp = x9 >> 8))))))))));
    mix((unsigned long) *lp);
}

int main(void)
{
    static const long v[] = { 0L, 1L, -1L, 0x12345678L, -0x12345678L,
                              0x7ffffff0L, -0x7ffffff0L };
    int i;

    for (i = 0; i < (int) (sizeof v / sizeof v[0]); i++) {
        through(lcell, qcell, fcell, v[i], (unsigned long long) v[i] * 3ULL + 11ULL);
        deep(&lcell[2], v[i]);
    }

    /* From gcc on the host, with long taken as 32 bits. */
    return h == 4233704612UL ? 42 : 1;
}
