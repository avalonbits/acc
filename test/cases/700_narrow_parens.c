/* A parenthesis on the way into a narrow destination.
 *
 * A store into a char may do its last operation in A, because nothing wider
 * will see the result. A parenthesis closing looked like that last
 * operation: `c = (x ^ y) & z` ran the `^` in A as if its result were going
 * straight into c, and loading z then overwrote it.
 */
int main(void) {
    int r = 0;
    unsigned char u = 200;
    short s = -3000;
    char c = 0;
    unsigned char *p = &u;

    *p = (10 ^ 134) & (s | u);
    if (u == 136) r = r + 1;

    c = (u + 1) & (s ^ 7);
    if (c == 9) r = r + 1;

    u = (c - 1) | (s & 4);
    if (u == 8) r = r + 1;

    /* 3 */
    return r + 39;
}
