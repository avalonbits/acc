/* Comparing longs, which is four bytes of subtraction.
 *
 * The last subtract of the four leaves the sign and overflow flags describing
 * the whole width, so the same branch sequence the 24-bit comparisons use
 * reads them unchanged. Equality cannot use that -- a subtract's zero flag
 * describes only the byte it was done on -- so it compares a byte at a time
 * and stops at the first difference.
 *
 * Unsigned ordering is the case that separates the two: 0xFFFFFFFF and -1 are
 * the same four bytes, and one is the largest unsigned long while the other
 * is less than zero.
 */
int main(void) {
    long a = 1000000;
    long b = 2000000;
    long neg = -1;
    unsigned long all_ones = 0xFFFFFFFF;
    unsigned long one = 1;
    long lo = 16777216;                 /* 0x01000000 */
    long hi = 33554432;                 /* 0x02000000 */
    int r = 0;

    if (a < b) r = r + 1;
    if (b > a) r = r + 1;
    if (a <= b) r = r + 1;
    if (b >= a) r = r + 1;
    if (a == 1000000) r = r + 1;
    if (a != b) r = r + 1;
    if (b < a) r = r + 1000;

    if (neg < a) r = r + 1;            /* signed: -1 is small */
    if (one < all_ones) r = r + 1;    /* unsigned: the same bits are large */
    if (all_ones < one) r = r + 1000;

    /* Differing only in the top byte, which a three-byte compare would miss. */
    if (lo < hi) r = r + 1;
    if (lo == hi) r = r + 1000;
    /* 9 */

    return r + 33;
}
