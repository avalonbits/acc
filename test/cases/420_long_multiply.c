/* Four-byte multiply.
 *
 * MLT is eight bits by eight, so the product is assembled from the partial
 * products of the bytes, and the pairs whose place value is already past bit
 * 31 are not computed at all. What that leaves out is the thing to test: a
 * product that needs the top byte, and one where the operands' high bytes
 * must not contribute to it.
 */
int main(void) {
    long small = 1000000;
    long seven = 7;
    long big = 100000;
    long r = 0;

    if (small * seven == 7000000) r = r + 1;

    /* Reaches into the top byte: 10^10 wraps to this in 32 bits. */
    if (big * big == 1410065408) r = r + 2;

    /* Every byte of both operands takes part. */
    if (16843009 * 3 == 50529027) r = r + 4;

    /* Negative operands are the same bits, so nothing special happens. */
    if (small * -3 == -3000000) r = r + 8;
    if (-small * -3 == 3000000) r = r + 16;
    if (small * 0 == 0) r = r + 32;
    if (small * 1 == 1000000) r = r + 64;

    /* 127 */
    return r - 85;
}
