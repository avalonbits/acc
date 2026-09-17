/* An unsigned comparison is not a signed one with the same operands.
 *
 * The compiler implements signed ordering by subtracting and repairing the
 * sign flag for overflow, and unsigned ordering by reading the carry. Pick
 * the wrong one and these come out backwards: as unsigned ints, 1 is less
 * than 16777215; as signed ints the same two bit patterns are 1 and -1, and
 * the answer is the other way about.
 */
int main(void) {
    unsigned int big = 16777215;    /* all twenty-four bits set */
    unsigned int one = 1;
    unsigned int half = 8388608;    /* 0x800000, the sign bit alone */
    int minus_one = -1;
    int positive = 1;
    int r = 0;

    if (one < big) r = r + 1;           /* unsigned: yes */
    if (big < one) r = r + 100;
    if (big > one) r = r + 2;
    if (one >= big) r = r + 100;
    if (one <= big) r = r + 4;

    if (minus_one < positive) r = r + 8;    /* signed: yes */
    if (positive < minus_one) r = r + 100;

    /* The halfway point, where the sign bit is the only thing set. As a
       signed int that is the most negative number there is. */
    if (one < half) r = r + 16;
    if (half < one) r = r + 100;
    /* 31 */

    return r + 11;
}
