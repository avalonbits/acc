/* Hexadecimal floating constants without a point.
 *
 * C99 6.4.4.2 makes the point optional when there is a binary exponent:
 * 0x1p-126 is the smallest normal float, and <float.h> is where a program
 * meets one. acc read the digits of a hex constant as an integer's and
 * refused the p as a bad digit -- only a point sent it to the floating
 * reader. A hex constant's own digits run through e and f, so those must
 * still be read as an integer's. */
int main(void) {
    int r = 0;
    float tiny = 0x1p-126f;

    if (0x1p-126f == 0x1.0p-126f && tiny > 0) r++;
    if (0x3P2 == 12.0f && 0xAp0f == 10.0f && 0x1p+4 == 16.0f) r++;
    if (0x1fp1 == 62.0f && 0xep-1 == 7.0f) r++;
    if (0x1f == 31 && 0xe == 14 && 0xfe == 254) r++;     /* still integers */

    return r + 38;              /* 4 checks */
}
