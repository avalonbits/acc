/* Pointer arithmetic, where the step is the width of what is pointed at.
 *
 * `p + 1` is the next object and not the next byte, so the integer side is
 * multiplied by that width, and `q - p` is the same thing backwards: a
 * difference in bytes divided down into a count of objects.
 *
 * The two halves are checked here by playing them off against each other.
 * `q = p + 5` then `q - p` comes to 5 whether both scalings are there or
 * neither, because the multiply and the divide cancel -- but it comes to 15
 * if only one of them is, which is what this catches. That the step is three
 * bytes rather than some other number needs two objects at a known distance
 * to see, and with no arrays and no casts the only pair available is two
 * locals, whose order is not something agondev can be asked to agree about.
 * test/self/010_pointer_step.c covers it against acc alone.
 */
int through(int *p) { return *p; }
int write_through(int *p) { *p = 42; return 0; }

int main(void) {
    int n = 42;
    int *p = &n;
    int *q;
    char c = 7;
    char *cp = &c;
    char *cq;
    long w = 1000000;
    long *wp = &w;
    int r = 0;

    /* Adding and taking back has to land where it started, at every width. */
    p = p + 3;
    p = p - 3;
    if (*p == 42) r = r + 1;
    cp = cp + 9;
    cp = cp - 9;
    if (*cp == 7) r = r + 1;
    wp = wp - 2;
    wp = wp + 2;
    if (*wp == 1000000) r = r + 1;

    /* A difference is in objects, so it is the number that was added -- and
     * would be three times it if the divide were missing. */
    q = p + 5;
    if (q - p == 5) r = r + 1;
    cq = cp + 5;
    if (cq - cp == 5) r = r + 1;

    /* Addition commutes. Subtraction does not, and acc refuses a pointer on
     * the right of one rather than quietly picking an order. */
    q = 2 + p;
    if (q - p == 2) r = r + 1;

    /* Comparison is on the addresses, unsigned. */
    if (p < q) r = r + 1;
    if (q > p) r = r + 1;
    if (p != q) r = r + 1;
    if (p == p) r = r + 1;

    /* A pointer is the same pointer on the other side of a call, going in
     * and coming back. */
    if (through(&n) == 42) r = r + 1;
    n = 0;
    write_through(&n);
    if (n == 42) r = r + 1;

    /* 12 */
    return r + 30;
}
