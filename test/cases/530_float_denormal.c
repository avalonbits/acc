/* Denormals: the numbers below the smallest one with a leading bit of its own.
 *
 * A float's significand has an implied 1 in front of it, which leaves no way
 * to write anything smaller than 2^-126 -- except that an exponent field of
 * zero is defined to mean the 1 is not there. That fills the gap between the
 * smallest normal and zero with evenly spaced values, so that subtracting two
 * nearby small numbers gives their difference instead of zero.
 *
 * Nothing in the arithmetic has a denormal case. A denormal at exponent zero
 * and a normal at exponent one are on the same scale, so unpacking a denormal
 * as exponent one with its leading bit left clear makes every routine treat
 * it like any other number; only packing has to notice, and it notices by
 * looking at whether the leading bit came back.
 */
int main(void) {
    float tiny = 1.1754943508222875e-38;    /* 2^-126, the smallest normal */
    float sub = 5.877471754111438e-39;      /* half of it, a denormal */
    int n = 0;

    if (tiny * 0.5 == sub) n = n + 1;
    if (sub > 0.0) n = n + 1;               /* not flushed to zero */
    if (sub < tiny) n = n + 1;
    if (sub + sub == tiny) n = n + 1;       /* and back up to a normal */
    if (tiny - sub == sub) n = n + 1;

    /* The smallest denormal of all, 2^-149, and one step above it. */
    if (1.401298464324817e-45 > 0.0) n = n + 1;
    if (2.802596928649634e-45 / 2.0 == 1.401298464324817e-45) n = n + 1;

    /* Below that there is nothing left, so it does reach zero. */
    if (1.401298464324817e-45 * 0.25 == 0.0) n = n + 1;
    if (tiny * 1.0e-30 == 0.0) n = n + 1;

    /* Two nearby normals subtract to a denormal rather than to zero, which
     * is the whole reason for having them. */
    if (tiny * 1.5 - tiny == sub) n = n + 1;

    /* 10 */
    return n + 32;
}
