/* Constants wider than a register, worked out by the compiler rather than
 * by the program: a global's initial value may be any of them, and inside a
 * function they cost no code at all. */
long g_sum = 2L + 3L;
long g_shift = 1L << 20;
long g_neg = -5L;
long g_cast = (long) 7;
long g_mixed = 1000L * 3L / 100L;
unsigned long g_mask = 0xffffffffUL & 0xff00ff00UL;
long long g_big = 1LL << 40;
long long g_div = 100000000000LL / 1000000LL;
float g_fadd = 1.5f + 2.25f;
float g_fmul = 0.5f * 3.0f;
float g_fint = 3;
long g_from_int = 3;
int g_cmp = 5L > 3L;

int main(void) {
    long a;
    long long q;
    float f;

    if (g_sum != 5L || g_shift != 1048576L || g_neg != -5L || g_cast != 7L)
        return 1;
    if (g_mixed != 30L || g_mask != 0xff00ff00UL)
        return 2;
    if (g_big != 1099511627776LL || g_div != 100000LL)
        return 3;
    if (g_fadd != 3.75f || g_fmul != 1.5f || g_fint != 3.0f)
        return 4;
    if (g_from_int != 3L || g_cmp != 1)
        return 5;

    /* The same inside a function, where what matters is that the answers
     * are right whether the compiler worked them out or the program did. */
    a = 2L + 3L;
    if (a != 5L)
        return 6;
    a = 1L << 20;
    if (a != 1048576L || (a >> 18) != 4L)
        return 7;
    a = -7L / 2L;
    if (a != -3L || -7L % 2L != -1L)
        return 8;
    q = 1LL << 40;
    if (q != 1099511627776LL || q / 1024LL != 1073741824LL)
        return 9;
    f = 1.5f + 2.25f;
    if (f != 3.75f || 10.0f / 4.0f != 2.5f)
        return 10;
    if (!(5L > 3L) || (2L > 3L) || !(1.5f < 2.0f))
        return 11;

    /* A constant and a variable still meet the runtime, and agree with it. */
    a = 1L << 20;
    if (a + 1L != 1048577L)
        return 12;
    if (q - 1LL != 1099511627775LL)
        return 13;

    /* Division by zero is not folded: the compiler must not be the thing
     * that divides by it, and what the program does with it is the
     * runtime's business. This one is not run, only compiled. */
    if (a == 0L && a / (a - a) != 0L)
        return 14;

    return 42;
}
