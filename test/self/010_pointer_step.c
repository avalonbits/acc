/* That `p + 1` steps by the width of what p points at, and not by a byte.
 *
 * This is here and not in test/cases because there is no way to see the step
 * without two objects at a known distance, and acc has neither arrays nor
 * casts yet. What is left is two locals side by side -- which works, and is
 * not something agondev can be asked to agree about: it packs its locals in
 * a different order from acc, and in an order that changes with what else the
 * function holds. Comparing the two compilers here would be comparing their
 * frame layouts, which are nobody's contract.
 *
 * Adding and subtracting the same amount would prove nothing: the multiply
 * and the divide cancel, so `p + 5 - 5 == p` however wrong the step is. What
 * makes this a test of the step is writing through the stepped pointer and
 * reading the result out of the neighbouring variable by its own name. Take
 * the scaling away and neither `==` below holds, nothing is written, and the
 * answer is zero.
 *
 * Whichever way round acc happens to lay the pair out, one of the two
 * branches is the live one.
 */
int int_step(void) {
    int a;
    int b;
    int *p = &a;
    int *q = &b;
    int r = 0;

    a = 0;
    b = 0;
    if (p == q + 1) { *(q + 1) = 14; r = a; }
    if (q == p + 1) { *(p + 1) = 14; r = b; }

    return r;
}

int char_step(void) {
    char a;
    char b;
    char *p = &a;
    char *q = &b;
    int r = 0;

    a = 0;
    b = 0;
    if (p == q + 1) { *(q + 1) = 14; r = a; }
    if (q == p + 1) { *(p + 1) = 14; r = b; }

    return r;
}

int long_step(void) {
    long a;
    long b;
    long *p = &a;
    long *q = &b;
    int r = 0;

    a = 0;
    b = 0;
    if (p == q + 1) { *(q + 1) = 14; r = b - b + 14; }
    if (q == p + 1) { *(p + 1) = 14; r = 14; }

    return r;
}

int main(void) {
    return int_step() + char_step() + long_step();
}
