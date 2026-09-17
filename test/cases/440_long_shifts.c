/* Four-byte shifts, and what decides their width.
 *
 * A shift is not one of the operators the usual arithmetic conversions apply
 * to. C99 6.5.7 promotes each operand on its own and gives the result the
 * type of the promoted left one, so a long count does not make the shift a
 * long shift. acc used to widen on either operand like every other binary
 * operator, which made `(unsigned) 0x400000 << 2L` come out as 16777216
 * where it has to wrap at 24 bits and give 0.
 *
 * The same rule says a right shift fills with the sign of the left operand
 * and never the count's, which is what the signed and unsigned pairs check.
 */
int main(void) {
    long a = 1000000;
    long neg = -1000000;
    unsigned long bits = 0xF0000000;
    unsigned int narrow = 4194304;      /* 0x400000, the top bit of an int */
    long count = 2;
    long r = 0;

    if (a << 3 == 8000000) r = r + 1;
    if (a >> 4 == 62500) r = r + 2;

    /* Signed right shift carries the sign down; unsigned brings in zero. */
    if (neg >> 4 == -62500) r = r + 4;
    if (bits >> 28 == 15) r = r + 8;
    if (neg >> 31 == -1) r = r + 16;

    /* Every bit out, which the count being masked to five bits is what
     * makes terminate. */
    if (a << 31 == 0) r = r + 32;

    /* The width is the left operand's. The count is a long in both, and in
     * the second the left operand is not, so the shift stays at int width
     * and wraps there. */
    if (a << count == 4000000) r = r + 64;
    if (narrow << count == 0) r = r + 128;

    /* 255 */
    return r - 213;
}
