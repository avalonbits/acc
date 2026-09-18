/* for loops: every clause present, each one left out, a declaration in the
 * first, and the loop variable's scope ending with the loop.
 */
int triangle(int n) {
    int sum = 0;
    int i;

    for (i = 1; i <= n; i++)
        sum += i;

    return sum;
}

/* for (;;) runs until something leaves it, which here is the return. */
int first_square_over(int limit) {
    int k = 0;

    for (;;) {
        if (k * k > limit)
            return k;
        k++;
    }
}

int main(void) {
    int r = 0;
    int i = 100;
    int n = 0;
    long big = 0;
    char c;

    if (triangle(4) == 10) r = r + 1;
    if (first_square_over(50) == 8) r = r + 1;

    /* A declaration in the init is its own variable: it shadows the i
     * above, and once the loop is over the name means that i again. */
    for (int i = 0; i < 3; i++)
        n += i;
    if (n == 3) r = r + 1;
    if (i == 100) r = r + 1;

    /* The same name declared by two loops in one function. */
    for (int i = 5; i > 0; i -= 2)
        n += i;
    if (n == 12) r = r + 1;

    /* No init, and no step: the body does the stepping. */
    n = 0;
    for (; n < 7;)
        n += 3;
    if (n == 9) r = r + 1;

    /* Nested, with the inner bound depending on the outer variable. */
    n = 0;
    for (int a = 0; a < 4; a++)
        for (int b = 0; b < a; b++)
            n++;
    if (n == 6) r = r + 1;

    /* A condition that is false at once: the body never runs, and the step
     * never runs either. */
    n = 0;
    for (i = 0; i > 5; n++)
        n = 99;
    if (n == 0 && i == 0) r = r + 1;

    /* A long counter, and a char one that wraps nothing on the way. */
    for (big = 70000; big < 70003; big++)
        n++;
    if (n == 3 && big == 70003) r = r + 1;
    for (c = 97; c <= 101; c++)
        n++;
    if (n == 8) r = r + 1;

    /* 10 */
    return r + 32;
}
