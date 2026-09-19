/* expect: 6: error: the constant does not fit in 64 bits */
/* Caught while the digits are being read, not after: the accumulator holds
 * sixty-four bits and twenty digits pass through it before the ladder ever
 * sees a value. */
int main(void) {
    unsigned long long n = 184467440737095516160;
    return (int) n;
}
