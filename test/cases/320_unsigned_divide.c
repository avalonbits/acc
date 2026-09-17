/* An unsigned divide is not a signed one on the same bits.
 *
 * 16777209 and -7 are the same twenty-four bits. Divided by 2 the first is
 * 8388604 and the second is -3, and a compiler that picked the wrong routine
 * would agree with itself everywhere except here.
 */
int main(void) {
    unsigned int big = 16777209;    /* the bits of -7 */
    unsigned int two = 2;
    int neg = -7;
    int stwo = 2;
    int r = 0;

    if (big / two == 8388604) r = r + 1;
    if (neg / stwo == -3) r = r + 2;
    if (big % two == 1) r = r + 4;
    if (neg % stwo == -1) r = r + 8;

    if (big / 3 == 5592403) r = r + 16;
    if (big % 3 == 0) r = r + 32;
    /* 63 */

    return r - 21;
}
