/* expect: 7: error: the constant does not fit in 32 bits */
/* Caught while the digits are being read, not after: the accumulator is four
 * bytes and eleven digits pass through it before the ladder ever sees a
 * value. 170_constant_too_large is the other half -- ten digits, which fit
 * the accumulator and are refused by the ladder. */
int main(void) {
    long n = 99999999999;
    return n;
}
