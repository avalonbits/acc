/* long: four bytes, wider than any register.
 *
 * So a long never lives in one. It stays in the frame and the operations work
 * on it there -- HL points at the destination, DE at the other operand, and a
 * helper walks the four bytes. Anything that forces a long into a register is
 * asking for the int it converts to, which is its low three bytes.
 *
 * The values are all past what an int holds, so a compiler that quietly did
 * this at 24 bits would get every one of them wrong.
 */
int main(void) {
    long a = 1000000;
    long b = 2000000;
    long neg = -1000000;
    long big = 2000000000;
    int r = 0;

    if (a + b == 3000000) r = r + 1;
    if (b - a == 1000000) r = r + 1;
    if (a + neg == 0) r = r + 1;
    if (neg - a == -2000000) r = r + 1;

    if ((a & 65535) == 16960) r = r + 1;
    if ((a | 15) == 1000015) r = r + 1;
    if ((a ^ a) == 0) r = r + 1;

    /* Past 24 bits, where an int would have wrapped. 4000000000 is more than
       a long holds and C99 would call it a long long, so the sum is checked
       against a hex constant, which C99 does allow to be unsigned long. */
    if (big + big == 0xEE6B2800) r = r + 1;
    if (big - big == 0) r = r + 1;
    /* 9 */

    return r + 33;
}
