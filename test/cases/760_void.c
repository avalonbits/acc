/* void functions: called as statements, returning early with `return;`,
 * falling off the end, recursing, and taking pointers to change things.
 */
int total;
int calls;

void reset(void) {
    total = 0;
    calls = 0;
}

void add(int n) {
    total += n;
    calls++;
}

void add_positive(int n) {
    if (n <= 0)
        return;
    add(n);
}

void count_down(int n) {
    if (n == 0)
        return;
    add(1);
    count_down(n - 1);
}

void swap(int *a, int *b) {
    int t = *a;

    *a = *b;
    *b = t;
}

int main(void) {
    int r = 0;
    int x = 3;
    int y = 7;

    add(5);
    add_positive(-4);
    add_positive(10);
    if (total == 15 && calls == 2) r = r + 1;

    count_down(4);
    if (total == 19 && calls == 6) r = r + 1;

    swap(&x, &y);
    if (x == 7 && y == 3) r = r + 1;

    reset();
    if (total == 0 && calls == 0) r = r + 1;

    /* In a for loop's clauses, where a statement's value is dropped too. */
    for (reset(); calls < 3; add(2))
        ;
    if (total == 6) r = r + 1;

    /* 5 */
    return r + 37;
}
