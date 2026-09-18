/* Decimal constants either side of where the lexer stops accumulating in 24
 * bits and carries on in 32.
 *
 * The first digits go into an unsigned for as long as another one cannot
 * carry it past 24 bits -- while it is below 1677721 -- and the rest into a
 * four-byte value. On the host an unsigned is 32 bits and a mistake in that
 * bound does nothing, so what these constants check on the host is only that
 * the two halves join up; test/target.sh has the Agon's acc compile this too,
 * which is where a bound that let the 24 bits overflow would show.
 *
 * Each constant is checked against one built from smaller ones by arithmetic,
 * in long where it has to be.
 */
int main(void) {
    int r = 0;
    long ten = 10;

    if (1677720 == 1677719 + 1) r = r + 1;
    if (1677721 == 1677720 + 1) r = r + 1;

    /* 1677720 is under the bound, so its last digit is still taken in 24
     * bits: 16777209 is the largest value that can be. */
    if (16777209 == 1677720 * ten + 9) r = r + 1;

    /* 1677721 is not, so the last digit here goes through the long loop. */
    if (16777210 == 1677721 * ten) r = r + 1;
    if (16777215 == 1677721 * ten + 5) r = r + 1;
    if (16777216 == 1677721 * ten + 6) r = r + 1;
    if (16777219 == 1677721 * ten + 9) r = r + 1;

    /* Past 24 bits entirely, and to the end of long. */
    if (99999999 == 9999999 * ten + 9) r = r + 1;
    if (167772159 == 16777215 * ten + 9) r = r + 1;
    if (2147483647 == 214748364 * ten + 7) r = r + 1;
    if (2147483647 > 2147483646) r = r + 1;

    /* 11 */
    return r + 31;
}
