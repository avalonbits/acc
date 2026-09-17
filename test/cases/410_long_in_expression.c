/* More than one long operation in a single expression.
 *
 * The long routines take the addresses of their two operands in HL and DE,
 * loaded with `lea` -- which the register allocator is never told about,
 * because an address is not a value it will ever be asked to produce. So
 * anything it had left in DE was gone by the time the routine was called, and
 * the first half of `(a == 1) + (b == 2)` was overwritten by the setup for
 * the second. One long operation per statement never showed it.
 */
int main(void) {
    long a = 1000000;
    long b = 2000000;
    long c = 3000000;
    int r = 0;

    r = (a == 1000000) + (b == 2000000) * 2 + (c == 3000000) * 4;

    /* Arithmetic as well as comparison, and nested deeply enough that a
     * result has to survive two more calls. */
    if (a + b == c) r = r + 8;
    if (a + b + c == 6000000) r = r + 16;
    if ((a < b) + (b < c) + (c < a) == 2) r = r + 32;

    /* 63 */
    return r - 21;
}
