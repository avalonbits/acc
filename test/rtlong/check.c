/* The runtime's long routines that take registers (lib/rt/lr*.s), against
 * acc's long operators, which call the ones that take addresses: the same
 * operands through both, the answers the same, and each routine keeping BC,
 * D and IY. glue.s makes them callable and leaves what they kept in t_bc,
 * t_d and t_iy, and the flags in t_flags: 1 the carry, 2 zero.
 *
 * The operands: every pair of the values at the edges of a byte, of three
 * bytes and of four, and as many pairs again from a generator. Answers 42
 * where all agree; otherwise says which failed, and answers the number of
 * the operation. */
#include <stdio.h>

long t_add(long, long), t_sub(long, long), t_and(long, long);
long t_or(long, long), t_xor(long, long), t_mul(long, long);
long t_divu(long, long), t_divs(long, long), t_remu(long, long);
long t_rems(long, long), t_cmpu(long, long), t_cmps(long, long);
long t_shl(long, int), t_shru(long, int), t_shrs(long, int);
long t_neg(long), t_not(long);

int t_iy, t_bc;
unsigned char t_d, t_flags;

static const long edges[] = {
    0, 1, -1, 2, 0x7f, 0x80, 0xff, 0x100, 0x7fff, 0x8000, 0xffff,
    0x7fffff, 0x800000, 0xffffff, 0x1000000, 0x12345678,
    0x7fffffff, (long) 0x80000000UL, (long) 0x80000001UL, -0x800000, -2
};
#define NEDGES ((int) (sizeof edges / sizeof edges[0]))

static unsigned long seed = 12345;

static long next(void)
{
    seed = seed * 1103515245UL + 12345;
    return (long) (seed ^ (seed << 11));
}

static int failed;

/* What the routine kept: BC as it was handed it, D and IY as glue.s set. */
static int kept(long bc)
{
    return t_d == 0x5a && t_iy == 0x123456 && t_bc == (int) (bc & 0xffffff);
}

static void bad(int op, long a, long b, long got, long want)
{
    if (!failed)
        failed = op;
    printf("op %d: %lx, %lx: %lx, not %lx\r\n", op, (unsigned long) a,
           (unsigned long) b, (unsigned long) got, (unsigned long) want);
}

#define CHECK(op, call, want, bc)                       \
    do {                                                \
        long got_ = (call);                             \
        if (got_ != (want) || !kept(bc))                \
            bad(op, a, b, got_, (want));                \
    } while (0)

static void pair(long a, long b)
{
    unsigned long ua = (unsigned long) a, ub = (unsigned long) b;
    int flags;

    CHECK(1, t_add(a, b), a + b, b);
    CHECK(2, t_sub(a, b), a - b, b);
    CHECK(3, t_and(a, b), a & b, b);
    CHECK(4, t_or(a, b), a | b, b);
    CHECK(5, t_xor(a, b), a ^ b, b);
    CHECK(6, t_mul(a, b), (long) (ua * ub), b);
    if (b != 0) {
        CHECK(7, t_divu(a, b), (long) (ua / ub), b);
        CHECK(9, t_remu(a, b), (long) (ua % ub), b);
        if (!(a == (long) 0x80000000UL && b == -1)) {
            CHECK(8, t_divs(a, b), a / b, b);
            CHECK(10, t_rems(a, b), a % b, b);
        }
    }

    /* A comparison leaves both as they were, and says it in the flags. */
    CHECK(11, t_cmpu(a, b), a, b);
    flags = (ua < ub) | (ua == ub) << 1;
    if (t_flags != flags)
        bad(11, a, b, t_flags, flags);
    CHECK(12, t_cmps(a, b), a, b);
    flags = (a < b) | (a == b) << 1;
    if (t_flags != flags)
        bad(12, a, b, t_flags, flags);
}

static void one(long a)
{
    long b = 0;
    int n;

    for (n = 0; n < 32; n++) {
        b = n;
        CHECK(13, t_shl(a, n), (long) ((unsigned long) a << n), 0x654321);
        CHECK(14, t_shru(a, n), (long) ((unsigned long) a >> n), 0x654321);
        CHECK(15, t_shrs(a, n), a >> n, 0x654321);
    }
    CHECK(16, t_neg(a), (long) (0UL - (unsigned long) a), 0x654321);
    CHECK(17, t_not(a), ~a, 0x654321);
}

int main(void)
{
    int i, j;

    for (i = 0; i < NEDGES; i++) {
        one(edges[i]);
        for (j = 0; j < NEDGES; j++)
            pair(edges[i], edges[j]);
    }
    for (i = 0; i < 400; i++) {
        long a = next(), b = next();

        one(a);
        pair(a, b);
        pair(a, b >> (i & 31));         /* a smaller divisor, too */
    }

    return failed ? failed : 42;
}
