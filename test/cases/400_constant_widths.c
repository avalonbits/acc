/* Where a decimal constant changes type, and where the accumulator that
 * reads one is close to overflowing.
 *
 * C99 types a constant by the first type that can hold it, so the same digits
 * mean an int up to 8388607 and a long past it, and the lexer accumulates
 * into four bytes to find out. The step either side of each boundary is what
 * says the ladder is on the right rungs; 2147483647 is the largest decimal
 * constant there is a type for, and it is one step from the guard that stops
 * the accumulator wrapping.
 */
int main(void) {
    long max_long = 2147483647;
    long near_max = 2147483646;
    long past_int = 8388608;            /* the first that is no longer an int */
    int  max_int = 8388607;             /* the last that still is */
    unsigned long hex_top = 0xFFFFFFFF;
    int r = 0;

    if (max_long > near_max) r = r + 1;
    if (max_long - near_max == 1) r = r + 2;
    if (past_int - max_int == 1) r = r + 4;
    if (hex_top > max_long) r = r + 8;  /* unsigned, so the top bits are size */
    if (near_max > max_long) r = r + 1000;

    /* The rung itself. 8388608 is one past the largest int, so C99 types it
     * long and this sum is exact; were it still an int the fold would happen
     * at 24 bits and come out zero. That is the only way the boundary shows
     * from inside the language -- a constant's type is not otherwise
     * something a program can ask about. */
    if (8388608 + 8388608 == 16777216) r = r + 16;
    if (8388608 + 8388608 == 0) r = r + 1000;
    /* 31 */

    return r + 11;
}
