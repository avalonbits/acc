/* The scratch area, given back as soon as nothing wants it.
 *
 * Every value the compiler moves out of a register goes to a slot in the
 * frame. The slots used to be handed out in order and all released together
 * at the end of the statement, so a statement's scratch was as large as
 * everything it ever spilled rather than as large as it holds at its worst
 * moment -- and (ix+d) reaches 128 bytes, so a statement with a handful of
 * eight-byte values in it could not be compiled at all. A slot is free when
 * nothing on the value stack is in it, which is when it is given back now.
 *
 * The first check is a statement that wants more than 128 bytes of scratch
 * if none of it comes back, and the answer says the slots that were handed
 * out twice were not both wanted at once.
 *
 * The second is what the reuse has to be careful of. A `?:` parks its middle
 * operand in a slot and nothing on the value stack says so, because the
 * third operand is compiled next and the stack has to be clear for it. A
 * second `?:` inside that third operand would be handed the same slot. That
 * is not a made-up shape: it is how printf reads an argument whose width the
 * format string gave -- `longs > 1 ? va_arg(ap, long long) : longs ?
 * va_arg(ap, long) : va_arg(ap, int)` -- and it is what failed when the
 * scratch a `?:` is holding was not marked as held.
 */
static long long q(long long v) { return v + 1LL; }
static long long f8(void) { return 800000000000LL; }
static long f4(void) { return 40000L; }
static int f3(void) { return 3; }

static int p6(int a, int b, int c, int d, int e, int f) {
    return a * 100000 + b * 10000 + c * 1000 + d * 100 + e * 10 + f;
}

static int keep(int n) { return n; }

int main(void) {
    int r = 0, i = 3;
    long long x = 5LL;

    /* One statement, six eight-byte terms, each of them a call and two
     * multiplications: more scratch than the frame has unless it is reused. */
    if (q(x * 1LL) * 2LL + q(x * 2LL) * 3LL + q(x * 3LL) * 4LL
        + q(x * 4LL) * 5LL + q(x * 5LL) * 6LL + q(x * 6LL) * 7LL == 587LL) r++;

    /* A `?:` inside the third operand of another, giving a long long: the
     * inner one must not be handed the slot the outer parked its middle in.
     * Each of the three ways through it. */
    {
        int n = 1;
        long long v = n > 1 ? f8() : n ? (long long) f4() : (long long) f3();

        if (v == 40000LL) r++;
        n = 2;
        v = n > 1 ? f8() : n ? (long long) f4() : (long long) f3();
        if (v == 800000000000LL) r++;
        n = 0;
        v = n > 1 ? f8() : n ? (long long) f4() : (long long) f3();
        if (v == 3LL) r++;
    }

    /* Six arguments, each of them a conditional: the shape a call takes the
     * most scratch in. */
    if (p6((i & 1) ? 1 : 2, (i & 2) ? 3 : 4, (i & 4) ? 5 : 6,
           (i & 8) ? 7 : 8, (i & 16) ? 9 : 1, (i & 32) ? 2 : 3)
        == 136813) r++;

    /* A long chain in one statement, where every term is still wanted when
     * the next is worked out. */
    if (keep(1) + keep(2) * keep(3) + keep(4) * keep(5) * keep(6)
        + keep(7) + keep(8) * keep(9) == 1 + 6 + 120 + 7 + 72) r++;

    /* Nested conditionals that park and then go on spilling. */
    {
        long a = i > 0 ? (i > 1 ? 100L : 200L) : (i > 2 ? 300L : 400L);
        long b = i < 0 ? 1L : (x > 5LL ? 2L : (i > 2 ? 3L : 4L));

        if (a == 100L && b == 3L) r++;
    }

    return r + 35;              /* 7 checks */
}
