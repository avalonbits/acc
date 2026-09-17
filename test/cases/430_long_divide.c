/* Four-byte division, both signs, and the remainder with it.
 *
 * Restoring long division, thirty-two iterations. The remainder needs
 * thirty-three bits, not thirty-two: it stays below the divisor, so doubling
 * it can reach 2*(2^32-1)+1, and a divisor above 2^31 makes that overflow.
 * The bit that falls out of the top is kept and says the subtract fitted
 * whatever the borrow said -- so a divisor above 2^31 is the case to cover,
 * and it needs an unsigned long to hold one.
 *
 * C99 has division truncate towards zero and the remainder take the sign of
 * the dividend, which is what all four sign combinations are here for.
 *
 * Division by zero is not here: it is undefined, so what the other compiler
 * does with it is not a reference for what acc should. acc's routine leaves
 * zero rather than looping, which is all that is required of it.
 */
int main(void) {
    long a = 1000000;
    long seven = 7;
    unsigned long top = 0xFFFFFFFF;
    unsigned long half = 0x80000001;
    long r = 0;

    if (a / seven == 142857) r = r + 1;
    if (a % seven == 1) r = r + 1;

    /* Truncation towards zero, and the remainder following the dividend. */
    if (-a / seven == -142857) r = r + 1;
    if (-a % seven == -1) r = r + 1;
    if (a / -seven == -142857) r = r + 1;
    if (a % -seven == 1) r = r + 1;
    if (-a / -seven == 142857) r = r + 1;
    if (-a % -seven == -1) r = r + 1;

    /* A divisor above 2^31, where the remainder needs its thirty-third bit. */
    if (top / half == 1) r = r + 1;
    if (top % half == 2147483646) r = r + 1;
    if (top / 3 == 1431655765) r = r + 1;

    /* 11 */
    return r + 31;
}
