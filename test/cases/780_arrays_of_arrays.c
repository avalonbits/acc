/* Arrays of arrays: local and global, two and three dimensions, of narrow,
 * wide and floating elements, initialised with nested braces, with braces
 * left out, and in part -- the gaps zeroed wherever they fall -- and passed
 * to functions whole or a row at a time.
 */
int grid[2][3] = {{1, 2, 3}, {4, 5, 6}};
int flat[][2] = {1, 2, 3, 4, 5};                    /* 3 rows, the last half full */
char cube[2][2][2] = {{{1, 2}, {3, 4}}, {{5, 6}, {7, 8}}};
long wide[2][2] = {{100000, -100000}, {70000}};

int total(int m[][3], int rows) {
    int sum = 0;

    for (int i = 0; i < rows; i++)
        for (int j = 0; j < 3; j++)
            sum += m[i][j];

    return sum;
}

int row_sum(int *row, int n) {
    int sum = 0;

    for (int i = 0; i < n; i++)
        sum += row[i];

    return sum;
}

/* Leaves its frame full of 55s, so that a partly initialised array in the
 * next call at the same depth shows its gaps if they are not zeroed. */
int dirty(void) {
    int junk[16];

    for (int i = 0; i < 16; i++)
        junk[i] = 55;

    return junk[15];
}

int sparse(void) {
    int m[3][4] = {{1}, {0, 2}, {0, 0, 3}};
    int sum = 0;

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            sum = sum * 2 + m[i][j];

    return sum;
}

int main(void) {
    int r = 0;
    int m[3][3];
    short s[2][3] = {{-1, -2, -3}, {4, 5, 6}};
    float f[2][2] = {{0.5, 1.5}, {2.5}};
    int cells[2][2][3];
    int n = 0;

    if (total(grid, 2) == 21 && grid[1][2] == 6) r = r + 1;
    if (flat[2][0] == 5 && flat[2][1] == 0 && flat[1][1] == 4) r = r + 1;
    if (cube[1][0][1] == 6 && cube[0][1][0] == 3) r = r + 1;
    if (wide[0][1] == -100000 && wide[1][0] == 70000 && wide[1][1] == 0) r = r + 1;

    /* Written element by element, then read back whole and by rows. */
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            m[i][j] = i * 10 + j;
    if (total(m, 3) == 99 && row_sum(m[2], 3) == 63) r = r + 1;

    /* Steps and compound assignment on elements. */
    m[1][1]++;
    m[2][0] += 5;
    --m[0][2];
    if (m[1][1] == 12 && m[2][0] == 25 && m[0][2] == 1) r = r + 1;

    if (s[0][2] + s[1][1] == 2) r = r + 1;
    if (f[0][1] + f[1][0] == 4.0 && f[1][1] == 0.0) r = r + 1;

    /* Three dimensions, indexed by variables. */
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++)
            for (int c = 0; c < 3; c++)
                cells[a][b][c] = a * 100 + b * 10 + c;
    for (int a = 0; a < 2; a++)
        n += cells[a][1][2];
    if (n == 124) r = r + 1;

    /* Gaps in the middle of a partly initialised array, zeroed over a dirty
     * stack: 1000 0200 0030 read as the digits of a number in base 2, which
     * any 55 left in a gap would change. */
    dirty();
    if (sparse() == 2182) r = r + 1;

    /* 10 */
    return r + 32;
}
