/* A negative long constant, widened to a long long.
 *
 * An integer literal too large for a 24-bit int and small enough for a
 * 32-bit long is a long, so -1405039608 is the negation of a long. acc keeps
 * a long constant as its four bytes, and handed those same four bytes to a
 * long long without their sign: `long long x = -1405039608` came out as
 * 2889927688. Found by gcc's shiftdi-2, whose table of expected shifts has
 * eight entries of exactly that size -- the eight shifts that failed -- and
 * by 20090711-1, which divides -990000000.
 */
long long g1 = -1405039608;
long long g2[3] = { -1405039608, -5488436, -2810079215 };
unsigned long long g3 = 3000000000;

static long long half(long long v) { return v / 32768; }

int main(void) {
    int r = 0;
    long long l = -1405039608;

    /* In an initialiser, global and local, alone and in a list. */
    if (g1 == -1405039608LL && l == -1405039608LL) r++;
    if (g2[0] == -1405039608LL) r++;

    /* The sizes either side of it were right, and still are: an int, a
     * long long, and an unsigned long whose top bit is not a sign. */
    if (g2[1] == -5488436LL && g2[2] == -2810079215LL) r++;
    if (g3 == 3000000000ULL) r++;

    /* In an expression. */
    if (half(-990000000) == -30212LL) r++;
    if (-1405039608 + 0LL < 0 && (long long) -1405039608 == -1405039608LL) r++;

    /* And the largest and smallest longs. */
    if ((long long) -2147483647L == -2147483647LL) r++;
    if ((long long) 2147483647L == 2147483647LL) r++;

    return r + 34;              /* 8 checks */
}
