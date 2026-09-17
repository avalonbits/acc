/* An assignment is an expression, and its value is what was stored -- after
 * the conversion to the destination's type, not before it.
 *
 * The bytes written to a narrow object are truncated by the store whether or
 * not the compiler converts first, so every other test here passes either
 * way. It is only when the assignment's own value is used that the difference
 * shows: `(c = 300)` is 44, not 300.
 */
int main(void) {
    char c;
    unsigned char uc;
    short s;
    int r = 0;

    if ((c = 300) == 44) r = r + 1;         /* 300 does not fit: 44 */
    if ((uc = 300) == 44) r = r + 2;
    if ((s = 100000) == -31072) r = r + 4;  /* nor does 100000 in a short */
    if ((c = -200) == 56) r = r + 8;

    /* And the value carries on into the rest of the expression. */
    r = r + ((c = 200) + 56);               /* 200 as a char is -56 */
    /* 15 + 0 = 15 */

    return r + 27;
}
