/* `cond ? middle : third`.
 *
 * Only one side is evaluated, and the two meet at one type -- decided by both
 * together, which is only known once the second has been parsed. So the first
 * side parks its value and jumps forward to a stub written afterwards, where
 * its conversion to the common type can finally be emitted. `c ? 1 : big`
 * with big a long is a long on both paths, not an int on one of them.
 *
 * It groups to the right, binds more loosely than || and more tightly than
 * assignment, and a pointer can meet a literal 0, which is the null pointer
 * spelled the way C spells it.
 */
int bump(int *count) {
    *count = *count + 1;

    return *count;
}

int main(void) {
    int yes = 1;
    int no = 0;
    long big = 16777216;
    long got;
    float f;
    int x = 42;
    int *p;
    int calls = 0;
    int r = 0;

    if ((yes ? 40 : 7) == 40) r = r + 1;
    if ((no ? 7 : 2) == 2) r = r + 1;

    /* Right to left: this is `x == 1 ? 10 : (x == 42 ? 42 : 99)`. */
    if ((x == 1 ? 10 : x == 42 ? 42 : 99) == 42) r = r + 1;

    /* Only the side chosen runs. */
    yes ? bump(&calls) : bump(&calls) + bump(&calls);
    if (calls == 1) r = r + 1;

    /* The common type, on both paths: int with long, int with float, long
     * with float. */
    got = no ? 1 : big;
    if (got == 16777216) r = r + 1;
    got = yes ? 1 : big;
    if (got == 1) r = r + 1;
    f = yes ? 2 : 0.5;
    if (f == 2.0) r = r + 1;
    f = no ? 0.5 : 3;
    if (f == 3.0) r = r + 1;

    /* A pointer and the null pointer. */
    p = yes ? &x : 0;
    if (*p == 42) r = r + 1;
    p = no ? &x : 0;
    if (!p) r = r + 1;

    /* The condition is true by value at its own type. */
    if ((big ? 1 : 0) == 1) r = r + 1;

    /* Looser than ||, tighter than =. */
    x = no || yes ? 5 : 6;
    if (x == 5) r = r + 1;

    /* 12 */
    return r + 30;
}
