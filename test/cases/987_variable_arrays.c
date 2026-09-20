/* Arrays whose length the program works out: `int a[n];`. The room comes
 * off the stack where the declaration stands and goes back at the end of
 * the block, so a loop that declares one does not take it again on every
 * turn; sizeof reads the size the declaration put beside it. */
static int total(const int *p, int n) {
    int i, s = 0;

    for (i = 0; i < n; i++)
        s = s + p[i];

    return s;
}

static int by_length(int n) {
    int a[n];
    int i;

    for (i = 0; i < n; i++)
        a[i] = i + 1;

    return total(a, n);
}

int main(void) {
    int n = 5;
    int a[n];
    int i, r = 0;

    for (i = 0; i < n; i++)
        a[i] = i * 2;

    if (total(a, n) != 20 || a[4] != 8)
        return 1;
    if ((int) sizeof a != n * (int) sizeof(int))
        return 2;
    if ((int) (sizeof a / sizeof a[0]) != 5)
        return 3;

    /* Element types other than int, and the pointer it decays to. */
    {
        int m = 3;
        char b[m];
        int *p = a;
        long w[m];

        b[0] = 'x';
        b[2] = 'z';
        w[0] = 100000L;
        w[2] = -1L;
        if (b[0] != 'x' || b[2] != 'z' || (int) sizeof b != 3)
            return 4;
        if (w[0] != 100000L || w[2] != -1L
            || (int) sizeof w != 3 * (int) sizeof(long))
            return 5;
        p[1] = 9;
        if (a[1] != 9)
            return 6;
    }

    /* One a turn, and the room given back each time: this would take a
     * thousand arrays' worth of stack if it did not. */
    for (i = 0; i < 200; i++) {
        int c[n];

        c[0] = i;
        c[n - 1] = i + 1;
        if (c[0] != i || c[n - 1] != i + 1)
            return 7;
        if (i == 100)
            continue;
    }

    /* Leaving the block early, by break and by return. */
    for (i = 0; i < 50; i++) {
        int d[n];

        d[0] = i;
        if (d[0] == 3)
            break;
    }
    if (i != 3)
        return 8;

    if (by_length(4) != 10 || by_length(1) != 1)
        return 9;

    /* The length is worked out where the declaration is, so it may be any
     * expression, and changing what it came from afterwards is nothing to
     * the array. */
    {
        int k = 2;
        int e[k * 3 - 1];

        k = 99;
        if ((int) sizeof e != 5 * (int) sizeof(int))
            return 10;
        e[4] = 7;
        if (e[4] != 7)
            return 11;
    }

    r = 42;

    return r;
}
