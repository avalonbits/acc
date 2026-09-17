/* Multiplication.
 *
 * MLT is the only multiplier the chip has and it is 8x8 -> 16, so a 24-bit
 * product is built from the partial products of the bytes -- six of them, the
 * pairs that land entirely above bit 23 being left out. Getting one of the
 * carries between the bytes wrong shows up only in products large enough to
 * reach the byte above, which is what the larger operands here are for.
 */
int mul(int a, int b) { return a * b; }

int main(void) {
    int r = 0;

    if (mul(1234, 5678) == 7006652) r = r + 1;
    if (mul(-37, 1234) == -45658) r = r + 2;
    if (mul(0, 12345) == 0) r = r + 4;
    if (mul(12345, 1) == 12345) r = r + 8;
    if (mul(-1, -1) == 1) r = r + 16;

    /* Wrapping, and the carries between all three bytes. */
    if (mul(100000, 100000) == 779264) r = r + 32;
    if (mul(4096, 4096) == 0) r = r + 64;           /* 2^24 wraps to nothing */
    /* 0xFF * 0x010101 is every bit set, which is -1 as a signed int. Written
       as -1 and not as 16777215: C99 makes a decimal constant that large a
       long, which acc has not got and makes unsigned instead, so the two
       compilers disagree about that spelling and agree about this one. */
    if (mul(255, 65793) == -1) r = r + 128;
    /* 255 */

    /* Precedence: * binds tighter than + and -. */
    if (2 + 3 * 4 == 14) r = r + 256;
    if (mul(2, 3) - 1 == 5) r = r + 512;
    /* 1023 */

    return r - 981;
}
