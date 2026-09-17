/* Dividing floats.
 *
 * Both significands are twenty-four bits with the leading one set, so their
 * ratio is between a half and two and the quotient needs at most one place of
 * shifting. Which case it is decides both the exponent's bias and how many
 * bits the loop has to produce, so a dividend below its divisor and one above
 * it are two different paths: 1.0 / 4.0 and 4.0 / 2.0 take one each.
 *
 * Twenty-four bits are produced, then one more as the guard, and whether the
 * remainder is empty afterwards is what separates a true tie from a value
 * just above half. 1.0 / 3.0 is the case with a remainder that never runs
 * out.
 */
int main(void) {
    int n = 0;

    if (4.0 / 2.0 == 2.0) n = n + 1;        /* dividend above divisor */
    if (1.0 / 4.0 == 0.25) n = n + 1;       /* below */
    if (3.0 / 2.0 == 1.5) n = n + 1;
    if (2.5 / 0.5 == 5.0) n = n + 1;

    /* Never exact, so the rounding is what is being checked. */
    if (1.0 / 3.0 == 0.3333333432674408) n = n + 1;
    if (2.0 / 3.0 == 0.6666666865348816) n = n + 1;

    /* Signs. */
    if (-6.0 / 3.0 == -2.0) n = n + 1;
    if (-6.0 / -3.0 == 2.0) n = n + 1;

    /* Zero over anything is zero and is answered before the loop. */
    if (0.0 / 5.0 == 0.0) n = n + 1;

    /* Dividing and multiplying back, which only comes out where the
     * division was exact. */
    if (1000000.0 / 1000.0 == 1000.0) n = n + 1;
    if (1.0 / 1024.0 * 1024.0 == 1.0) n = n + 1;

    if (1.0 / 1.0 == 1.0) n = n + 1;

    /* 12 */
    return n + 30;
}
