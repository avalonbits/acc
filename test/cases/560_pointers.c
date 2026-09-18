/* Pointers: taking an address, reading through one, and writing through one.
 *
 * A pointer here is the three bytes an int is, so it lives in a register like
 * one and needs no widening anywhere. What it needs is a width to read and
 * write through it, and that is the type it points at -- which is why the
 * type byte carries a pointer depth over a base type rather than a separate
 * kind. `int *` is a depth of one over int and `char **` a depth of two over
 * char, and the width of the pointer itself is never stored, because every
 * pointer on this machine is the same size.
 *
 * Reading through a pointer widens exactly as a load from a frame slot does,
 * so a signed char read through a `char *` comes back sign-extended and an
 * unsigned one does not.
 */
int main(void) {
    int n = 42;
    int *p = &n;
    char c = 7;
    char *cp = &c;
    short h = 300;
    short *hp = &h;
    int **pp = &p;
    int r = 0;

    if (*p == 42) r = r + 1;
    if (*cp == 7) r = r + 1;
    if (*hp == 300) r = r + 1;
    if (**pp == 42) r = r + 1;

    /* Writing, at each width. */
    *p = 1;
    if (n == 1) r = r + 1;
    *cp = 9;
    if (c == 9) r = r + 1;
    *hp = 400;
    if (h == 400) r = r + 1;
    **pp = 2;
    if (n == 2) r = r + 1;

    /* An assignment is an expression, so it has the value it stored. */
    if ((*p = 5) == 5) r = r + 1;
    if (n == 5) r = r + 1;

    /* 10 */
    return r + 32;
}
