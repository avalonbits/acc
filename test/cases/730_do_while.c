/* do-while: the body runs before the test, so at least once. */
int count_digits(long n) {
    int digits = 0;

    do {
        digits++;
        n = n / 10;
    } while (n != 0);

    return digits;
}

int main(void) {
    int r = 0;
    int i = 10;
    int j;
    int n = 0;

    /* Once, although the condition is false from the start. */
    do
        n++;
    while (i < 5);
    if (n == 1) r = r + 1;

    if (count_digits(0) == 1 && count_digits(7) == 1) r = r + 1;
    if (count_digits(1000000000) == 10) r = r + 1;

    /* Nested, with a compound body and a condition with a side effect. */
    n = 0;
    i = 0;
    do {
        j = 0;
        do {
            n += j;
        } while (++j < 3);
    } while (i++ < 3);
    if (n == 12 && i == 4) r = r + 1;

    /* 4 */
    return r + 38;
}
