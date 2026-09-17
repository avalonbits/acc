/* The operators the chip has no instruction for.
 *
 * AND, OR and XOR are eight bits wide here and the upper byte of a register
 * has no name; there is no barrel shifter, so a shift is a loop; MLT is the
 * only multiplier and it is 8x8; and there is no divide at all. acc carries
 * the routines and puts the ones this program uses into it.
 *
 * Prints 00002A -- 42. */
int main(void) {
    int a = 986895;         /* 0x0F0F0F */
    int b = 65280;          /* 0x00FF00 */
    int r = 0;

    if ((a & b) == 3840) r = r + 1;
    if ((a | b) == 1048335) r = r + 2;
    if ((a ^ b) == 1044495) r = r + 4;
    if ((b << 4) == 1044480) r = r + 8;
    if ((b >> 4) == 4080) r = r + 16;
    if (1234 * 5678 == 7006652) r = r + 32;
    if (-7 / 2 == -3) r = r + 64;       /* towards zero, as C99 says */
    if (-7 % 2 == -1) r = r + 128;      /* the dividend's sign */

    return r - 213;
}
