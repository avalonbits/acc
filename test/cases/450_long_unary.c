/* Unary minus and complement on a long.
 *
 * Both used to go through the 24-bit path -- `0 - x` in HL, and ~x as -x - 1
 * -- which reaches three of the four bytes. A constant was worse: it was
 * folded through a truncation to 24 bits, so `-1574716585` became a number
 * that fits in three bytes and nothing afterwards could recover it.
 */
int main(void) {
    long a = 1000000;
    long wide = 1574716585;     /* needs all four bytes */
    unsigned long bits = 0x0F0F0F0F;
    long r = 0;

    if (-a == -1000000) r = r + 1;
    if (-(-a) == 1000000) r = r + 1;
    if (~a == -1000001) r = r + 1;
    if (-0 == 0) r = r + 1;

    /* The cases the 24-bit path could not reach. */
    if (-wide < 0) r = r + 1;
    if (-wide + wide == 0) r = r + 1;
    if (-wide == -1574716585) r = r + 1;
    if (~bits == 0xF0F0F0F0) r = r + 1;
    if (~wide == -1574716586) r = r + 1;

    /* 9 */
    return r + 33;
}
