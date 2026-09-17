/* Infinities.
 *
 * An exponent past the top used to give the largest finite float, on the
 * reasoning that it was wrong by less. It is the worse answer: a program that
 * goes on to compute with it gets a number that looks ordinary, where an
 * infinity carries the fact that something overflowed through the rest of the
 * arithmetic and comes out the far end still saying so.
 *
 * The representation is the one the format already reserves: every exponent
 * bit set and an empty mantissa, which is why unpacking leaves the implied
 * leading 1 off at that exponent -- it is what tells an infinity from a NaN.
 */
int main(void) {
    float huge = 3.4028234663852886e38;     /* the largest finite float */
    float inf = huge * 2.0;
    int n = 0;

    if (inf > huge) n = n + 1;
    if (inf == huge * 4.0) n = n + 1;       /* one infinity, however reached */
    if (huge + huge == inf) n = n + 1;
    if (-huge * 2.0 == -inf) n = n + 1;
    if (-inf < -huge) n = n + 1;

    /* Arithmetic on an infinity, where the answer is defined. */
    if (inf + 1.0 == inf) n = n + 1;
    if (inf * 2.0 == inf) n = n + 1;
    if (inf / 2.0 == inf) n = n + 1;
    if (1.0 / inf == 0.0) n = n + 1;
    if (-1.0 / inf == 0.0) n = n + 1;
    if (1.0 / 0.0 == inf) n = n + 1;
    if (-1.0 / 0.0 == -inf) n = n + 1;
    if (inf * 0.5 == inf) n = n + 1;

    /* And it stays out of reach of anything finite. */
    if (inf > 1.0e38) n = n + 1;
    if (huge < inf) n = n + 1;

    /* 15 */
    return n + 27;
}
