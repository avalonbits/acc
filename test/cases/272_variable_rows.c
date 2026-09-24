/* Arrays whose rows the program sizes as it runs: `int M[n][m]`, `int
 * A[a][b][c]`, `int B[3][n]`, a typedef of one, `sizeof` of each part, and
 * a pointer to a row, `int (*p)[m]`. C99 6.7.5.2 has every dimension
 * variable; acc took only the first, since a row's size is how far a
 * subscript steps. It is now kept in the frame beside the array, where the
 * declaration put it, and a step over such a row multiplies by it.
 * 20221006-1 and 20040411-1 are the torture tests. */
static int grid(int n, int m)
{
    int M[n][m], i, j, s = 0;

    for (i = 0; i < n; i++)
        for (j = 0; j < m; j++)
            M[i][j] = i * m + j;
    for (i = 0; i < n; i++)
        for (j = 0; j < m; j++)
            s += M[i][j] == i * m + j;

    return s == n * m && sizeof M == n * m * sizeof(int)
           && sizeof M[1] == m * sizeof(int) && sizeof M[1][2] == sizeof(int)
           && &M[2][0] - &M[1][0] == m && M + 2 - M == 2;
}

static int cube(int a, int b, int c)
{
    int A[a][b][c], i, j, k, ok = 1;

    for (i = 0; i < a; i++)
        for (j = 0; j < b; j++)
            for (k = 0; k < c; k++)
                A[i][j][k] = (i * 10 + j) * 10 + k;
    for (i = 0; i < a; i++)
        for (j = 0; j < b; j++)
            for (k = 0; k < c; k++)
                ok &= A[i][j][k] == (i * 10 + j) * 10 + k;

    return ok && sizeof A[1] == b * c * sizeof(int);
}

static int mixed(int n)
{
    int B[3][n], C[n][4], i;

    for (i = 0; i < n; i++) {
        B[2][i] = i;
        C[i][3] = -i;
    }

    return B[2][n - 1] == n - 1 && C[n - 1][3] == 1 - n
           && sizeof B == 3 * n * sizeof(int) && sizeof C == n * 4 * sizeof(int);
}

static int typed(int n)
{
    typedef int row[n + 2];
    row r;
    int k;

    n = 100;                    /* the typedef's size was taken above */
    for (k = 0; k < (int) (sizeof r / sizeof r[0]); k++)
        r[k] = k;

    return sizeof(row) == 7 * sizeof(int) && sizeof r == sizeof(row) && r[6] == 6;
}

static int loop(void)
{
    int t, ok = 1;

    for (t = 1; t < 50; t++) {
        char buf[t][t];

        buf[t - 1][t - 1] = (char) t;
        ok &= buf[t - 1][t - 1] == (char) t;
    }

    return ok;
}

static int pointer(void)
{
    int n = 3, m = 4, M[n][m], ok = 1;
    int (*p)[m] = M;
    int (*q)[m] = &M[2];

    M[1][2] = 7;
    M[2][3] = 9;
    ok &= p[1][2] == 7 && (*q)[3] == 9 && q - p == 2;
    p++;
    ok &= (*p)[2] == 7 && sizeof *p == m * sizeof(int);
    ok &= sizeof(int (*)[m]) == sizeof(void *) && sizeof(int[n][m]) == sizeof M;

    return ok;
}

int main(void)
{
    int r = 0;

    if (grid(4, 4) && grid(3, 5)) r++;
    if (cube(2, 3, 4)) r++;
    if (mixed(5)) r++;
    if (typed(5)) r++;
    if (loop()) r++;
    if (pointer()) r++;

    return r + 36;              /* 6 checks */
}
