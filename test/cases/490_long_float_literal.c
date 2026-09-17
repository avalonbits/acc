/* A floating literal with more digits than an integer can hold.
 *
 * The lexer reads the digits as an integer first and only backs out when the
 * character they stop at says the number was floating after all. That is what
 * makes an ordinary integer cost one pass over its digits instead of two, and
 * it is why the guard against the accumulator wrapping cannot refuse on the
 * spot: eleven digits is too many for a long and perfectly ordinary in front
 * of a decimal point. It notes the overflow and only complains once the
 * terminator has said this was an integer.
 */
int main(void) {
    float wide = 12345678901234.5;
    int r = 0;

    /* A float keeps about seven digits, so all fourteen of these do not
     * survive -- the point is that the literal is read at all, and lands
     * where a number of that size belongs. */
    if (wide > 12300000000000.0) r = r + 1;
    if (wide < 12400000000000.0) r = r + 1;
    if (wide == 12345679020032.0) r = r + 1;

    /* The digits either side of the point both count, so a hundred of them
     * is still one number. */
    if (0.000000000000000000000000000000000000011754944 > 0.0) r = r + 1;

    /* 4 */
    return r + 38;
}
