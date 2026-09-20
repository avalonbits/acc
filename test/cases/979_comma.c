/* The comma operator: the left side for what it does, the right for what it
 * is. Where C says `expression` -- a statement, the clauses of a for, what
 * is inside ( ) or [ ], the middle of ?:, what follows return -- and not
 * where a comma separates arguments, initial values or declarators. */
int calls;

int bump(int by) {
    calls = calls + by;

    return calls;
}

int two(int a, int b) { return a * 10 + b; }

int main(void) {
    int a = 1, b = 2, c = 3;
    int t[4];
    int i, j, n;

    /* As a statement: both sides run, in order. */
    a = 10, b = 20;
    if (a != 10 || b != 20)
        return 1;

    /* The value is the right side, and the left still happens. */
    calls = 0;
    a = (bump(1), bump(2), 7);
    if (a != 7 || calls != 3)
        return 2;

    /* In a for: two counters, one loop. */
    n = 0;
    for (i = 0, j = 5; i < j; i++, j--)
        n++;
    if (n != 3 || i != 3 || j != 2)
        return 3;

    /* In a subscript, in a condition, and after return through a call. */
    t[0] = 9; t[1] = 8; t[2] = 7; t[3] = 6;
    if (t[(0, 2)] != 7)
        return 4;
    calls = 0;
    i = 0;
    while (bump(1), i < 2)
        i++;
    if (i != 2 || calls != 3)
        return 5;

    /* The middle of ?:, which C makes a whole expression. */
    calls = 0;
    a = c ? (bump(5), 11) : 12;
    if (a != 11 || calls != 5)
        return 6;

    /* A comma between arguments is a separator, not this operator -- and one
     * inside a parenthesised argument is the operator again. */
    if (two(1, 2) != 12 || two((1, 3), 4) != 34)
        return 7;

    /* And between declarators, where the initial values are their own. */
    {
        int x = bump(0), y = x + 1;

        if (y != x + 1)
            return 8;
    }

    /* Nested, and left to right: the answer is the last one. */
    a = 0;
    b = (a = 1, a = a + 1, a + 10);
    if (a != 2 || b != 12)
        return 9;

    /* In a do-while's condition, and as a discarded statement value. */
    calls = 0;
    i = 0;
    do {
        i++;
    } while (bump(2), i < 3);
    if (i != 3 || calls != 6)
        return 10;

    return 42;
}
