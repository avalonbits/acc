/* & | ^ on 24-bit values.
 *
 * None of these is an instruction here. AND, OR and XOR are eight bits wide,
 * and the upper byte of HL has no name, so two thirds of a register can be
 * reached and the rest cannot -- which is why they go to a helper that puts
 * both operands on the stack, where every byte of them has an address.
 *
 * The constants are chosen so every byte of the result differs from every
 * byte of the operands: a helper that dropped or duplicated one of the three
 * would pass on values whose bytes happen to agree.
 */
int main(void) {
    int a = 986895;         /* 0x0F0F0F */
    int b = 65280;          /* 0x00FF00 */
    int c = 16711935;       /* 0xFF00FF, negative as a signed int */
    int r = 0;

    if ((a & b) == 3840) r = r + 1;             /* 0x000F00 */
    if ((a | b) == 1048335) r = r + 2;          /* 0x0FFF0F */
    if ((a ^ b) == 1044495) r = r + 4;          /* 0x0FF00F */
    if ((a & c) == 983055) r = r + 8;           /* 0x0F000F */
    if ((a | c) == -61441) r = r + 16;          /* 0xFF0F0F, negative */
    if ((b ^ c) == -1) r = r + 32;              /* 0xFFFFFF */

    /* Precedence: & binds tighter than ^, and ^ tighter than |. The operands
       are picked so the two groupings differ -- with `1` on the right, both
       `(a & b) | 1` and `a & (b | 1)` come to the same number and the test
       says nothing. */
    if ((a & b | c) == -61441) r = r + 64;      /* (a&b)|c is 0xFF0F00 */
    if ((a | b ^ c) == -1) r = r + 128;         /* a|(b^c) is 0xFFFFFF */
    /* 255 */

    return r - 213;
}
