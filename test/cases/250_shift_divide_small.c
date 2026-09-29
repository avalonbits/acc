/* A right shift by a constant of 1 to 8, and a divide by 2 to 256, which
 * acc sends to the runtime's shr and sdiv in place of the loops: A:HL
 * shifted left by 8 - k, its top three bytes taken, and one added to a
 * negative quotient that had bits shifted out. Each is checked against the
 * same operation by a divisor the compiler cannot see, which still takes
 * the loop, over values whose top byte has its high bit both clear and
 * set, and exactly divisible or not. An unsigned remainder by a power of
 * two, now a mask, is checked the same way, and so is an unsigned divide by
 * 2^16 to 2^23, now the top byte's shift.
 */
static unsigned vals[] = {
    0x000000, 0x000001, 0x0000ff, 0x000100, 0x00ffff, 0x010000, 0x123456,
    0x7fffff, 0x800000, 0x800001, 0x9abcde, 0xfedcba, 0xffff00, 0xffffff
};

static int by(int k)
{
    return 1 << k;
}

#define ONE(k)                                                          \
    bad += (u >> k) != u / (unsigned) by(k);                            \
    bad += u / (1u << k) != u / (unsigned) by(k);                       \
    bad += u % (1u << k) != u % (unsigned) by(k);                       \
    bad += s / (1 << k) != s / by(k);                                   \
    bad += (s >> k) != (s - (s & (by(k) - 1))) / by(k);

static int check(unsigned u)
{
    int s = (int) u;
    int bad = 0;

    ONE(1) ONE(2) ONE(3) ONE(4) ONE(5) ONE(6) ONE(7) ONE(8)

    bad += u / 65536u != u / (unsigned) by(16);
    bad += u / 0x800000u != u / (unsigned) by(23);

    /* An unsigned char divisor promotes to int: the divide stays signed. */
    bad += s / (unsigned char) 4 != s / by(2);

    /* The shift keeps DE: a value held there across it is still there. */
    bad += (u >> 3) + (u >> 5) != u / 8u + u / 32u;

    return bad;
}

int main(void)
{
    int i, bad = 0;

    for (i = 0; i < (int) (sizeof vals / sizeof vals[0]); i++)
        bad += check(vals[i]);

    return bad ? 100 + bad : 42;
}
