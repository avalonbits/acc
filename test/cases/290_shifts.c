/* << and >> on 24-bit values.
 *
 * There is no barrel shifter, so a shift is a loop over the bits however
 * small the amount, and there is no way to shift all three bytes of HL right
 * at all -- the right shifts go through the stack a byte at a time.
 *
 * A right shift is the one place the operand's own signedness decides the
 * instruction rather than the usual conversions: `>>` on a signed negative
 * carries the sign down, and on an unsigned value it does not.
 */
int main(void) {
    int one = 1;
    int b = 65280;              /* 0x00FF00 */
    int neg = -256;
    unsigned int u = 16776960;  /* 0xFFFF00, the same bits as neg */
    int n = 4;
    int r = 0;

    if ((one << n) == 16) r = r + 1;
    if ((b << n) == 1044480) r = r + 2;         /* 0x0FF000 */
    if ((b >> n) == 4080) r = r + 4;            /* 0x000FF0 */

    /* The same twenty-four bits, shifted right as signed and as unsigned. */
    if ((neg >> n) == -16) r = r + 8;
    if ((u >> n) == 1048560) r = r + 16;        /* 0x0FFFF0 */

    /* Shifting the top bit out and back. */
    if ((one << 23) == -8388608) r = r + 32;

    /* Precedence: shifts bind tighter than the comparisons and looser
       than + -, so this is (b >> (n + 4)) and not ((b >> n) + 4). */
    if (b >> n + 4 == 255) r = r + 64;
    /* 127 */

    return r - 85;
}
