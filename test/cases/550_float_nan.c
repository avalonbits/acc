/* NaNs: the answer where there is no answer.
 *
 * Infinity minus infinity, zero times infinity and zero over zero are the
 * three that have no value a number could stand for -- not zero, not an
 * infinity, not anything in between. IEEE gives them a representation of
 * their own: every exponent bit set, as an infinity has, and a mantissa that
 * is not empty.
 *
 * What a NaN costs elsewhere is the comparison. It is not below, equal to or
 * above anything, itself included, so `x < y` and `x >= y` are both false
 * when either operand is one. Three outcomes cannot say that, which is why
 * comparing floats returns a code -- below, equal, above, unordered -- rather
 * than leaving flags for a branch to read the way the integer comparisons do.
 */
int main(void) {
    float inf = 3.4028234663852886e38 * 2.0;
    float nan = inf - inf;
    int n = 0;

    /* The three operations with nothing to return. */
    if (inf - inf != inf - inf) n = n + 1;
    if (0.0 * inf != 0.0 * inf) n = n + 1;
    if (0.0 / 0.0 != 0.0 / 0.0) n = n + 1;
    if (inf / inf != inf / inf) n = n + 1;
    if (-inf + inf != -inf + inf) n = n + 1;

    /* Not equal to itself is the one comparison that holds. */
    if (nan != nan) n = n + 1;
    if (nan == nan) n = n + 1000;
    if (nan < 1.0) n = n + 1000;
    if (nan > 1.0) n = n + 1000;
    if (nan <= 1.0) n = n + 1000;
    if (nan >= 1.0) n = n + 1000;
    if (nan == 0.0) n = n + 1000;

    /* And it survives whatever is done to it. */
    if (nan + 1.0 != nan + 1.0) n = n + 1;
    if (nan * 2.0 != nan * 2.0) n = n + 1;
    if (nan / 2.0 != nan / 2.0) n = n + 1;
    if (1.0 / nan != 1.0 / nan) n = n + 1;
    if (-nan != -nan) n = n + 1;

    /* 11 */
    return n + 31;
}
