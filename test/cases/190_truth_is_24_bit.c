/* A condition is false only when all twenty-four bits are zero.
 *
 * The obvious test on this chip is the sixteen-bit one -- `ld a,l` then
 * `or a,h` -- because the upper byte of HL is not addressable. It is wrong
 * for exactly these values, and wrong quietly: every condition in every other
 * test here has something in its low sixteen bits, so nothing else would
 * notice a compiler that skipped the body of `if (65536)`.
 */
int main(void) {
    int r = 0;
    int n = 65536;
    int spins = 0;

    if (65536)          /* 0x010000 */
        r = r + 1;
    if (131072)         /* 0x020000 */
        r = r + 2;
    if (-8388608)       /* 0x800000, the most negative int here */
        r = r + 4;
    if (0)
        r = r + 1000;

    /* And as a loop condition, where getting it wrong skips the loop. */
    while (n) {
        spins = spins + 1;
        n = 0;
    }
    r = r + spins;

    return r + 34;
}
