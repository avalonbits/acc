/* An else-if chain, which is an else whose statement is an if.
 *
 * Every arm has to be reachable, or the test says nothing about the ones it
 * never enters. Three separate conditions rather than three tests of one
 * value, because with no comparison operators yet the only condition is "is
 * this nonzero", and `n`, `n + 1`, `n + 2` cannot be false in turn. */
int pick(int a, int b, int c) {
    if (a)
        return 1;
    else if (b)
        return 2;
    else if (c)
        return 3;
    else
        return 4;
}

int main(void) {
    int s = pick(1, 0, 0) + pick(0, 1, 0);

    s = s + pick(0, 0, 1) + pick(0, 0, 0);
    /* 1 + 2 + 3 + 4 = 10 */

    return s + 32;
}
