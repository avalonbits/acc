/* A float take away a constant, which acc does as the float plus the
 * constant with its sign turned over. That has to be the same answer as
 * taking the same value away as a variable, which goes through the
 * subtraction: bit for bit, for zeros of both signs, infinities, a NaN,
 * numbers too small to be normal and ones that round. The answers are
 * checked against each other rather than against the host's, whose float
 * routines are not the Agon's -- and not against agondev's, which works out
 * a constant take away a constant as the host does, so that its own two
 * answers disagree. And wide constants whose runs of three
 * bytes are alike -- zero, all ones, a byte repeated -- which are written
 * with the register loaded once. */
static unsigned long bits(float f)
{
    union { float f; unsigned long u; } b;

    b.f = f;
    if (f != f)
        return 0x7fc00000UL;            /* any NaN, as one */

    return b.u;
}

static int wrong;

#define SAME(x, c) \
    do { \
        volatile float as_var = (c); \
        if (bits((x) - (c)) != bits((x) - as_var)) \
            wrong++; \
    } while (0)

static void minus(float x)
{
    SAME(x, 0.0f); SAME(x, -0.0f); SAME(x, 1.0f); SAME(x, -2.5f);
    SAME(x, 1e30f); SAME(x, 1e-40f); SAME(x, 3.0e38f); SAME(x, 0.1f);
    SAME(x, 1.0f / 0.0f); SAME(x, -1.0f / 0.0f);
}

int main(void)
{
    static const float v[] = { 0.0f, -0.0f, 1.0f, -1.0f, 2.5f, 1e-40f,
                               -1e-40f, 3.0e38f, 1e30f, 0.3f };
    long zero = 0L, ones = -1L, same = 0x01010101L;
    long long qzero = 0LL, qones = -1LL;
    int i;

    for (i = 0; i < (int) (sizeof v / sizeof v[0]); i++) {
        float inf = 1.0f / 0.0f;

        minus(v[i]);
        minus(inf);
        minus(-inf);
        minus(inf - inf);
    }
    if (zero != 0 || ones != -1 || same != 16843009L)
        wrong++;
    if (qzero != 0 || qones != -1 || (unsigned long) (qones >> 32) != 0xffffffffUL)
        wrong++;

    return wrong == 0 ? 42 : 1;
}
