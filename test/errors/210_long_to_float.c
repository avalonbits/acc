/* expect: 7: error: converting a long to a floating-point type is not implemented yet */
/* An int converts to a float exactly -- twenty-four bits into a twenty-four
 * bit significand -- so that conversion needs no rounding and is done. A long
 * is thirty-two bits and does need it, and that is not written yet. */
int main(void) {
    long n = 1000000;
    float f = n;
    return f;
}
