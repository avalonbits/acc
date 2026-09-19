/* Declarations anywhere in a block, as C99 has them: after statements, in
 * nested blocks, in loops, shadowing an outer name and then letting it go.
 */
int total(int n) {
    int sum = 0;

    for (int i = 0; i < n; i++) {
        int square = i * i;         /* initialised again on every pass */

        sum += square;
    }

    return sum;
}

int main(void) {
    int r = 0;
    int x = 1;

    r = r + 1;
    int y = x + 1;                  /* after a statement */
    if (y == 2) r = r + 1;

    {
        int x = 10;                 /* shadows the outer x */

        if (x == 10) r = r + 1;
        {
            int x = 100;

            if (x == 100) r = r + 1;
        }
        if (x == 10) r = r + 1;     /* the inner one ended */
    }
    if (x == 1) r = r + 1;          /* and so did this one */

    if (total(4) == 14) r = r + 1;

    /* Arrays and strings in an inner block. */
    if (x) {
        char word[] = "block";
        int counts[3] = {1, 2, 3};

        if (word[4] == 'k' && counts[2] == 3) r = r + 1;
    }

    /* The same name again in a later block is a new variable. */
    {
        int fresh = 5;

        r = r + fresh - 4;
    }
    {
        int fresh = 7;

        if (fresh == 7) r = r + 1;
    }

    /* 10 */
    return r + 32;
}
