/* A right shift by a constant of 16 to 23, which acc writes out rather than
 * calls for: HL's top byte, reached through the stack, shifted by the rest
 * and widened -- with zeros for an unsigned value and with its sign for a
 * signed one. Each is checked against the same value by division, which
 * takes another way to it, over values whose top byte has its high bit
 * both clear and set, and against agondev for the answer.
 */
static unsigned vals[] = {
    0x000000, 0x00ffff, 0x010000, 0x123456, 0x7fffff,
    0x800000, 0x9abcde, 0xfedcba, 0xffffff
};

static int check(unsigned u)
{
    int s = (int) u;
    int bad = 0;

    bad += (u >> 16) != u / 65536u;
    bad += (u >> 17) != u / 131072u;
    bad += (u >> 20) != u / 1048576u;
    bad += (u >> 22) != u / 4194304u;
    bad += (u >> 23) != u / 8388608u;

    /* A signed shift rounds down, where division rounds toward zero: the
     * two agree once the bits shifted out are taken off first. */
    bad += (s >> 16) != (s - (s & 0xffff)) / 65536;
    bad += (s >> 19) != (s - (s & 0x7ffff)) / 524288;
    bad += (s >> 22) != (s - (s & 0x3fffff)) / 4194304;
    bad += (s >> 23) != (s < 0 ? -1 : 0);

    /* And the value is live afterwards: the shift took nothing else. */
    bad += ((u >> 16) + u) != u / 65536u + u;

    return bad;
}

int main(void)
{
    int i, bad = 0;
    unsigned sum = 0;

    for (i = 0; i < (int) (sizeof vals / sizeof vals[0]); i++) {
        bad += check(vals[i]);
        sum += (vals[i] >> 18) ^ (unsigned) ((int) vals[i] >> 21);
    }

    return bad ? 100 + bad : (sum & 0x3f) == 22 ? 42 : 1;
}
