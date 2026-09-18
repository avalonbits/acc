/* Octal constants: a leading 0 followed by more digits.
 *
 * C has always read these as base eight, and acc did not -- `010` was ten.
 * An octal constant is typed the way a hex one is, so it may be unsigned where
 * a decimal one of the same value would have to be long. A lone 0 is octal
 * too, by the grammar, and is zero either way.
 *
 * `09.5` is a decimal float, not a bad octal constant: which a string of
 * digits is shows only at the character they stop at.
 */
int main(void) {
    int r = 0;
    float f = 09.5;

    if (010 == 8) r = r + 1;
    if (0777 == 511) r = r + 1;
    if (0 == 0) r = r + 1;
    if (07 == 7) r = r + 1;
    if (0100 + 1 == 65) r = r + 1;

    /* Typed like hex: 040000000 is 2^23, past the largest int, and may be an
     * unsigned int where a decimal constant would have gone to long. */
    if (040000000 > 0) r = r + 1;

    if (f == 9.5) r = r + 1;
    if (01.5 == 1.5) r = r + 1;

    /* 8 */
    return r + 34;
}
