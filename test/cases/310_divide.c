/* Division and remainder, which the chip cannot do at all.
 *
 * C99 requires the quotient to truncate towards zero and the remainder to
 * take the sign of the dividend, so -7/2 is -3 and -7%2 is -1 -- not the
 * floor, which would be -4 and 1. Both signs of both operands are here
 * because the helper divides magnitudes and puts the sign back afterwards,
 * and there are four ways to get that wrong.
 */
int div(int a, int b) { return a / b; }
int rem(int a, int b) { return a % b; }

int main(void) {
    int r = 0;

    if (div(7, 2) == 3) r = r + 1;
    if (div(-7, 2) == -3) r = r + 2;        /* towards zero, not -4 */
    if (div(7, -2) == -3) r = r + 4;
    if (div(-7, -2) == 3) r = r + 8;

    if (rem(7, 2) == 1) r = r + 16;
    if (rem(-7, 2) == -1) r = r + 32;       /* the dividend's sign */
    if (rem(7, -2) == 1) r = r + 64;
    if (rem(-7, -2) == -1) r = r + 128;
    /* 255 */

    if (div(1000000, 3) == 333333) r = r + 256;
    if (rem(1000000, 3) == 1) r = r + 512;
    if (div(0, 5) == 0) r = r + 1024;
    if (div(8388607, 1) == 8388607) r = r + 2048;
    /* 4095 */

    return r - 4053;
}
