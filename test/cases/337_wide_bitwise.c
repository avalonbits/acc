/* AND, OR and XOR of ints wider than a byte, made by opt-acc's own
 * backend: a constant's bytes where the top byte is left as it is or an
 * AND clears it; two values of two bytes or less a byte at a time; and the
 * rest by the helper, with BC -- which may hold a value -- kept. Each is
 * checked on its own. */
static int helper(int x, int y)
{
    int right = 0;

    right += (x | y) == 0x337577;
    right += (x ^ y) == 0x216121;
    right += (x & y) == 0x121456;
    right += (0x123456 ^ x) == 0x0;
    right += (x ^ 0xff0000) == 0xed3456;         /* no byte form */

    return right;                                /* 5 */
}

static int constants(int x)
{
    int right = 0;

    right += (x & 0x00ff0f) == 0x003406;         /* AND clears the top */
    right += (x & 0x00ff00) == 0x003400;         /* nothing low */
    right += (x & 0x0000ff) == 0x56;
    right += (x & 0xfffffe) == 0x123456;         /* ~1: the top as it is */
    right += (x | 0x001234) == 0x123676;
    right += (0x00ff00 ^ x) == 0x12cb56;
    right += (x & 0x00ffff) + 1 == 0x3457;

    return right;                                /* 7 */
}

/* Masked where they are used, so that each side is known to be two bytes
 * wide -- through a variable, what it was masked with is not known. */
static int narrow(int x, int y)
{
    int right = 0;

    right += ((x & 0xff00) | (y & 0x00ff)) == 0x3477;
    right += ((x & 0xff00) ^ (y & 0xffff)) == 0x6177;
    right += ((x & 0xffff) & (y & 0xffff)) + 0x10000 == 0x11456;

    right += (x & (y & 0xffff)) == 0x1456;         /* x's top cleared */

    return right;                                /* 4 */
}

/* The right side in BC: `mask` lives across the loop. */
static int in_bc(const int *values, int n, int mask)
{
    int total = 0;

    while (n--)
        total = total + (*values++ & mask);

    return total;
}

int main(void)
{
    int values[3];
    int score = 0;

    values[0] = 0x111111;
    values[1] = 0x222222;
    values[2] = 0x444444;
    score += helper(0x123456, 0x335577) == 5;
    score += constants(0x123456) == 7;
    score += narrow(0x123456, 0x335577) == 4;
    score += in_bc(values, 3, 0x0f0f0f) == 0x070707;
    return score == 4 ? 42 : score;
}
