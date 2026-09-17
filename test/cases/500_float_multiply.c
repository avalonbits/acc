/* Multiplying floats.
 *
 * Two twenty-four bit significands make a forty-eight bit product, assembled
 * from the nine partial products of their bytes -- and unlike the integer
 * multiply, none of the nine can be dropped: the top of the product is
 * exactly the part that is kept.
 *
 * Both significands have their leading bit set, so the product lands in
 * [2^46, 2^48) and needs at most one shift. Which of the two cases it is
 * decides the exponent, and 1.5 * 1.5 against 1.5 * 2.0 is the pair that
 * tells them apart: the first stays below 2 and the second does not.
 */
int main(void) {
    float r = 0;
    int n = 0;

    if (1.5 * 2.0 == 3.0) n = n + 1;        /* reaches bit 47 */
    if (1.5 * 1.5 == 2.25) n = n + 1;       /* does not */
    if (0.5 * 0.5 == 0.25) n = n + 1;
    if (3.0 * 4.0 == 12.0) n = n + 1;

    /* Signs, which are the two signs differing and nothing else. */
    if (-2.0 * 3.0 == -6.0) n = n + 1;
    if (-2.0 * -3.0 == 6.0) n = n + 1;

    /* Zero has no leading 1 to multiply, so it is answered before the
     * product is ever formed. */
    if (1.0 * 0.0 == 0.0) n = n + 1;
    if (0.0 * 1000000.0 == 0.0) n = n + 1;

    /* Every byte of both significands takes part. */
    if (1000.0 * 1000.0 == 1000000.0) n = n + 1;
    if (1.1 * 1.1 == 1.2100000381469727) n = n + 1;

    /* An exponent that runs off the top saturates rather than making an
     * infinity, which is written down as a divergence from C99. */
    if (1.0e30 * 1.0e30 > 1.0e38) n = n + 1;

    if (r == 0.0) n = n + 1;

    /* 12 */
    return n + 30;
}
