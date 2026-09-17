/* The u and l suffixes.
 *
 * Without them there is no way to write a long constant that is small enough
 * to be an int, so `1L` did not lex at all and every long in a program had to
 * be built from a value large enough to type itself.
 *
 * C99 types a constant by the first type in a list that can hold it, and the
 * suffix says which list: u takes the signed types out, l takes int out. The
 * unsigned types are also in the list for a hex constant with no suffix,
 * which is the only place the decimal and hex forms differ.
 */
int main(void) {
    long one = 1L;
    unsigned long ubig = 4294967295u;
    long shifted = 1L << 24;        /* an int would have shifted it away */
    int r = 0;

    if (one == 1) r = r + 1;
    if (1L + 1L == 2) r = r + 2;
    if (shifted == 16777216) r = r + 4;
    if (ubig == 0xFFFFFFFF) r = r + 8;
    if (ubig > 0) r = r + 16;       /* unsigned, so the top bit is not a sign */

    /* Case and order do not matter, and 1U is an unsigned int rather than an
     * unsigned long, so this stays at int width and wraps. */
    if (4194304U << 2 == 0) r = r + 32;
    if (1UL == 1LU) r = r + 64;
    if (10l == 10L) r = r + 128;

    /* 255 */
    return r - 213;
}
