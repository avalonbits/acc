/* A parameter whose rows are as long as another parameter says, C99
 * 6.7.5.2 and 6.9.1p10: the lengths are worked out each time the function
 * is entered, so `a[i][j]` steps by what the call gave, `sizeof *a` is the
 * row's size, and a size that does something does it on entry. A size is
 * read again as it was written, macros, comments and all. */
#define TWICE(x) ((x) * 2)
static int sum(int n, int m, int a[][m])
{
    int i, j, s = 0;

    for (i = 0; i < n; i++)
        for (j = 0; j < m; j++)
            s += a[i][j];

    return s;
}

static int bump(int n, int a[n++])
{
    return n;
}

static int row_size(int n, int (*p)[n])
{
    return sizeof *p / sizeof (int);
}

static int cube(int n, int m, int a[][n][m])
{
    return a[1][1][2] + (int) (sizeof a[0] / sizeof (int));
}

/* A prototype's own sizes are not worked out: n here is nothing. */
static int apply(int (*f)(int n, int a[][n]), int k, int b[][k])
{
    return f(k, b);
}

static int first_of_second(int n, int a[][n])
{
    return a[1][0];
}

static int macro_row(int n, int a[][TWICE(n) /* ] */ ])
{
    return sizeof *a / sizeof (int) + a[1][0];
}

static int qualified(int n, int a[static const 1][n // ]
                                                 ])
{
    return a[1][1] + (int) sizeof "n";
}

int main(void)
{
    int a[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
    int b[4][2] = { { 1, 2 }, { 3, 4 }, { 5, 6 }, { 7, 8 } };
    int c[2][2][3] = { { { 0 } }, { { 0 }, { 0, 0, 9 } } };

    if (sum(2, 3, a) != 21 || sum(4, 2, b) != 36 || sum(3, 2, b) != 21)
        return 1;
    if (bump(4, 0) != 5)
        return 2;
    if (row_size(3, a) != 3 || row_size(2, b) != 2)
        return 3;
    if (cube(2, 3, c) != 9 + 6)
        return 4;
    if (apply(first_of_second, 2, b) != 3 || apply(first_of_second, 3, a) != 4)
        return 5;
    if (macro_row(2, b) != 4 + 5 || qualified(2, b) != 4 + 2)
        return 6;

    return 42;
}
