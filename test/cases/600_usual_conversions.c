/* The usual arithmetic conversions, where an operator's two operands are not
 * the same type.
 *
 * An integer meeting a float becomes a float, which is arithmetic rather than
 * a relabelling. acc used to refuse it -- `f + 1` was a conversion between
 * floating point and integer that was not implemented -- so a float could
 * only ever be combined with another float.
 *
 * Between integers, a long wins, and it is unsigned only when one of the two
 * was an unsigned long. An unsigned int is twenty-four bits here and a signed
 * long of thirty-two holds every one of them, so C converts that pair to a
 * plain long: `(unsigned) 1 + -2L` is -1 and less than zero. acc made it
 * unsigned long, which made it 4294967295.
 */
int main(void) {
    float f = 40.5;
    float g;
    int a = 3;
    long n = -2;
    unsigned int u = 1;
    unsigned long ul = 1;
    int r = 0;

    g = f + 1;
    if (g == 41.5) r = r + 1;
    g = a + f;
    if (g == 43.5) r = r + 1;
    g = f * a;
    if (g == 121.5) r = r + 1;
    g = a / 2.0;
    if (g == 1.5) r = r + 1;
    if (f > 40) r = r + 1;
    if (a < f) r = r + 1;

    g = n * 0.5;
    if (g == -1.0) r = r + 1;

    if (u + n < 0) r = r + 1;
    if (u + n == -1) r = r + 1;
    if (ul + a > 0) r = r + 1;

    /* 10 */
    return r + 32;
}
