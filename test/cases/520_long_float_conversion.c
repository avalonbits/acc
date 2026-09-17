/* Between a long and a float.
 *
 * An int converts exactly, because an int here is twenty-four bits and a
 * float keeps twenty-four of significand. A long is thirty-two and does not,
 * which is the whole reason this is a separate routine: 16777217 is the first
 * integer a float cannot hold, and the two either side of it round in
 * opposite directions.
 *
 * The other way truncates towards zero, as C says, and reaches values an int
 * cannot -- which is what a long is for.
 */
int main(void) {
    long big = 2000000000;
    long exact = 16777216;          /* 2^24, the last one that is exact */
    float f = big;
    long back = f;
    int n = 0;

    /* Rounded to nearest on the way in, so the round trip is not the
     * identity -- 2000000000 is not a float. */
    if (f == 2000000000.0) n = n + 1;
    if (back == 2000000000) n = n + 1;

    f = exact;
    back = f;
    if (back == 16777216) n = n + 1;

    /* The two either side of the first gap. 16777217 rounds down to an even
     * significand and 16777219 rounds up. */
    f = 16777217;
    back = f;
    if (back == 16777216) n = n + 1;

    f = 16777219;
    back = f;
    if (back == 16777220) n = n + 1;

    /* Negative, where the magnitude is taken and the sign put back. */
    f = -2000000000;
    back = f;
    if (back == -2000000000) n = n + 1;

    /* And out past what an int could hold either way. */
    f = 1.0e9;
    back = f;
    if (back == 1000000000) n = n + 1;

    if (2000000000.0 > 0.0) n = n + 1;

    /* 8 */
    return n + 34;
}
