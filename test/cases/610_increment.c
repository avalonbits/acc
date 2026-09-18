/* `++` and `--`, before and after, on every kind of thing that can change.
 *
 * Before, the answer is the new value; after, it is the value the operand
 * had before it changed -- which means taking a copy first, because a local
 * on the value stack is a description of where to find it, and reading it
 * after the store would read the new value.
 *
 * The step is 1 however wide the operand is: a pointer turns it into the
 * width of what it points at, and a float into 1.0, by the ordinary rules.
 *
 * Postfix binds tighter than a unary `*`, so `*p++` reads through p and then
 * moves p on, while `(*p)++` changes what p points at. Both are here, and
 * `++*p` besides.
 */
int step_along(int *p) {
    int first = *p++;           /* reads, then moves p */

    return first;
}

int main(void) {
    int x = 40;
    unsigned char c = 255;
    long n = 16777215;
    float f = 1.5;
    int count = 0;
    int *p = &count;
    int *q = p;
    int r = 0;

    if (x++ == 40) r = r + 1;
    if (x == 41) r = r + 1;
    if (++x == 42) r = r + 1;
    if (x-- == 42) r = r + 1;
    if (--x == 40) r = r + 1;

    /* Each at its own width: the byte wraps, the long crosses into its fourth
     * byte, the float steps by one. */
    c++;
    if (c == 0) r = r + 1;
    n++;
    if (n == 16777216) r = r + 1;
    f++;
    if (f == 2.5) r = r + 1;
    f--;
    if (f == 1.5) r = r + 1;

    /* Through a pointer, in all three spellings. */
    (*p)++;
    (*p)++;
    if (count == 2) r = r + 1;
    if (++*p == 3) r = r + 1;
    if (count == 3) r = r + 1;
    if (step_along(&count) == 3) r = r + 1;

    /* A pointer steps by what it points at, so the difference is one. */
    q++;
    if (q - p == 1) r = r + 1;
    q--;
    if (q == p) r = r + 1;

    /* The usual loop. */
    x = 0;
    count = 0;
    while (x < 5) {
        count += x;
        x++;
    }
    if (count == 10) r = r + 1;

    /* 16 */
    return r + 26;
}
