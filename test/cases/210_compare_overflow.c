/* Signed comparison where the subtraction that implements it overflows.
 *
 * `a < b` is done by subtracting and looking at the sign of the result, which
 * is the answer only when the subtraction did not overflow -- and when it did,
 * the answer is the opposite. There is no flag holding that condition and no
 * instruction producing it, so the compiler has to branch the two cases apart.
 *
 * It takes the extremes to get there. 8388607 is the largest int on this
 * machine and -8388608 the smallest, and their difference does not fit:
 *
 *   8388607 - (-8388608) = 16777215, which is -1 in 24 bits: sign set,
 *                          overflow set, and the true answer is "not less"
 *   -8388608 - 8388607   = -16777215, which is 1: sign clear, overflow set,
 *                          and the true answer is "less"
 *
 * Both are the opposite of what the sign flag says. Every other comparison in
 * the suite is between small numbers, where the subtraction cannot overflow
 * and the sign flag alone is right -- so nothing else here would notice a
 * compiler that trusted it.
 */
int main(void) {
    int big = 8388607;
    int small = -8388608;
    int r = 0;

    if (small < big) r = r + 1;
    if (big < small) r = r + 100;
    if (big > small) r = r + 2;
    if (small > big) r = r + 100;
    if (small <= big) r = r + 4;
    if (big >= small) r = r + 8;
    if (big <= small) r = r + 100;
    if (small >= big) r = r + 100;
    if (big != small) r = r + 16;
    if (big == small) r = r + 100;
    /* 1 + 2 + 4 + 8 + 16 = 31 */

    /* And the other overflowing pair: a large negative against a large
       positive that are not the extremes. */
    if (-8000000 < 8000000) r = r + 32;
    if (8000000 < -8000000) r = r + 100;

    return r - 21;
}
