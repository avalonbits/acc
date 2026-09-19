/* Converting a long double back to an integer, which has no oracle: agondev
 * compiles `(long) 1.5L` to a call to libagon's __dtol, and that routine
 * answers 0 -- for 1.5, for 2.0 and for 1e9 alike. So the values here are
 * checked against what C says they are and not against a second compiler.
 *
 * The other direction, and everything else a long double does, is in
 * test/cases/978_long_double.c, where agondev does agree. */
long double g = 1e9L;

long narrow(long double v) { return (long) v; }

int main(void) {
    long double x;

    x = 1.5L;       if ((long) x != 1L) return 1;
    x = 2.0L;       if ((long) x != 2L) return 2;
    x = 100.75L;    if ((long) x != 100L) return 3;
    x = -2.75L;     if ((long) x != -2L) return 4;   /* towards zero */
    x = 0.5L;       if ((long) x != 0L) return 5;
    x = -0.5L;      if ((long) x != 0L) return 6;
    x = 1000000.5L; if ((long) x != 1000000L) return 7;
    if (narrow(g) != 1000000000L) return 8;
    if ((int) (long) 12.75L != 12) return 9;
    if ((char) (long) 65.5L != 'A') return 10;

    /* Round trips through the type: every integer under 2^53 is exact, so
     * what goes in comes back. */
    if ((long) (long double) 0L != 0L) return 11;
    if ((long) (long double) 1L != 1L) return 12;
    if ((long) (long double) -1L != -1L) return 13;
    if ((long) (long double) 2147483647L != 2147483647L) return 14;
    if ((long) (long double) (-2147483647L - 1L) != -2147483647L - 1L) return 15;
    if ((unsigned long) (long double) 4000000000UL != 4000000000UL) return 16;

    return 42;
}
