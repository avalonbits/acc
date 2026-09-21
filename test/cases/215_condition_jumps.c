/* Comparisons that are conditions rather than values.
 *
 * acc branches on the flags a comparison's subtract leaves rather than
 * making a one and testing that against zero -- and this chip answers a
 * signed comparison in two pieces, because an overflowing subtract leaves
 * the sign the other way round. So every operator is walked here in every
 * shape a condition comes in, at values that overflow the subtract and at
 * values that do not, with agondev asked to agree about all of it.
 *
 * An int is three bytes here, so the ends are -8388608 and 8388607, and the
 * pairs below are chosen to land on both sides of them. */
#define LOW  (-8388607 - 1)
#define HIGH 8388607

static int lt(int a, int b) { if (a <  b) return 1; return 0; }
static int gt(int a, int b) { if (a >  b) return 1; return 0; }
static int le(int a, int b) { if (a <= b) return 1; return 0; }
static int ge(int a, int b) { if (a >= b) return 1; return 0; }
static int eq(int a, int b) { if (a == b) return 1; return 0; }
static int ne(int a, int b) { if (a != b) return 1; return 0; }

static int ult(unsigned a, unsigned b) { if (a <  b) return 1; return 0; }
static int uge(unsigned a, unsigned b) { if (a >= b) return 1; return 0; }

/* The other shapes: a loop's test, the two halves of `&&` and `||`, and the
 * question mark -- each of which asks the same question of the flags. */
static int count_up_to(int n)
{
    int i = 0, seen = 0;

    while (i < n) { seen++; i++; }
    for (i = 0; i > n; i--)
        seen++;

    return seen;
}

static int both(int a, int b) { return a < b && b < 0; }
static int either(int a, int b) { return a < b || b < 0; }
static int pick(int a, int b) { return a < b ? 7 : 9; }

int main(void)
{
    int r = 0;

    /* Neither side near the ends: the subtract cannot overflow. */
    if (lt(1, 2)) r++;
    if (!lt(2, 1)) r++;
    if (!lt(2, 2)) r++;
    if (gt(2, 1)) r++;
    if (!gt(1, 2)) r++;
    if (le(2, 2)) r++;
    if (le(1, 2)) r++;
    if (!le(2, 1)) r++;
    if (ge(2, 2)) r++;
    if (ge(2, 1)) r++;
    if (!ge(1, 2)) r++;
    if (eq(2, 2)) r++;
    if (!eq(1, 2)) r++;
    if (ne(1, 2)) r++;
    if (!ne(2, 2)) r++;

    /* And with the ends, where it does: HIGH - LOW and LOW - HIGH are both
     * outside an int, and this is the case a single `jp m` gets wrong. */
    if (lt(LOW, HIGH)) r++;
    if (!lt(HIGH, LOW)) r++;
    if (gt(HIGH, LOW)) r++;
    if (!gt(LOW, HIGH)) r++;
    if (le(LOW, HIGH)) r++;
    if (ge(HIGH, LOW)) r++;
    if (lt(LOW, 0)) r++;
    if (!lt(HIGH, 0)) r++;
    if (lt(-1, HIGH)) r++;
    if (!lt(HIGH, -1)) r++;
    if (ne(LOW, HIGH)) r++;
    if (eq(LOW, LOW)) r++;

    /* Unsigned, where the borrow is the whole answer and the ends are not
     * the same numbers: 0xffffff is the largest and not a negative one. */
    if (ult(1u, 2u)) r++;
    if (!ult(2u, 1u)) r++;
    if (ult(0u, 0xffffffu)) r++;
    if (!ult(0xffffffu, 0u)) r++;
    if (uge(0xffffffu, 1u)) r++;
    if (!uge(1u, 0xffffffu)) r++;

    /* The other shapes. */
    if (count_up_to(5) == 5) r++;
    if (count_up_to(-3) == 3) r++;
    if (count_up_to(0) == 0) r++;
    if (both(1, 2) == 0) r++;
    if (both(-3, -2) == 1) r++;
    if (either(1, 2) == 1) r++;
    if (either(2, 1) == 0) r++;
    if (pick(1, 2) == 7) r++;
    if (pick(2, 1) == 9) r++;

    return r;                            /* 42 checks */
}
