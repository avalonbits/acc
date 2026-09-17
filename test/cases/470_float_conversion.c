/* Between a float and an int, which is arithmetic and not a relabelling.
 *
 * Every int on this machine converts to a float exactly: a float keeps
 * twenty-four bits of significand counting the leading one it does not store,
 * and an int here is twenty-four bits wide, so there is never anything to
 * round away. That is why it is worth checking the ends of the range rather
 * than a few small numbers -- 8388607 is the one that fills the significand.
 *
 * The other direction truncates towards zero, as C says, and not towards
 * minus infinity, which is what shifting the significand down would give on
 * its own: -3.75 has to come back as -3 and not as -4.
 *
 * Unary minus on a float is its sign bit flipped. It is also how a negative
 * literal is built, because the lexer only ever produces a magnitude.
 */
int main(void) {
    int big = 8388607;              /* the largest int, and a full significand */
    int small = -8388608;
    float f = big;
    int a = f;
    int r = 0;

    if (a == 8388607) r = r + 1;

    f = small;
    a = f;
    if (a == -8388608) r = r + 2;

    f = 1000000;
    a = f;
    if (a == 1000000) r = r + 4;

    /* Towards zero from each side: 3 and -3, never 3 and -4. */
    f = 3.75;
    a = f;
    if (a == 3) r = r + 8;

    f = -3.75;
    a = f;
    if (a == -3) r = r + 16;

    /* Everything below one goes to zero, whichever side of it. */
    f = 0.0001;
    a = f;
    if (a == 0) r = r + 32;

    f = -0.9999;
    a = f;
    if (a == 0) r = r + 64;

    /* 127 */
    return r - 85;
}
