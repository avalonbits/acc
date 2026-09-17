/* Comparison binds looser than + and -, and equality looser than ordering.
   `a + 1 < b` is `(a + 1) < b`, and `a < b == c < d` is
   `(a < b) == (c < d)`. */
int main(void) {
    int a = 5;
    int b = 9;
    int c = 1;
    int d = 2;
    int r = 0;

    if (a + 4 == b)       r = r + 1;
    if (b - 4 == a)       r = r + 2;
    if (a + 1 < b)        r = r + 4;
    if (a < b == c < d)   r = r + 8;    /* 1 == 1 */
    if (a < b == d < c)   r = r + 100;  /* 1 == 0 */
    if (a > b == d < c)   r = r + 16;   /* 0 == 0 */
    if (a - a == 0)       r = r + 32;
    /* 1 + 2 + 4 + 8 + 16 + 32 = 63 */

    return r - 21;
}
