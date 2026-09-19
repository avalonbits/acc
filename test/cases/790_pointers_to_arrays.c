/* Pointers to whole arrays: stepping a row at a time, subscripted twice,
 * dereferenced back into a row, taken from an array with &, compared and
 * subtracted, passed to a function, and stored through with (*p)[i].
 */
int grid[3][4] = {{1, 2, 3, 4}, {5, 6, 7, 8}, {9, 10, 11, 12}};

int last_of(int (*row)[4]) {
    return (*row)[3];
}

int main(void) {
    int r = 0;
    int (*p)[4] = grid;
    int (*q)[4];
    int a[4] = {10, 20, 30, 40};
    int (*whole)[4] = &a;
    int *first;

    /* One step is one row: four ints. */
    p++;
    if (p[0][0] == 5 && p[1][2] == 11) r = r + 1;

    /* *p is the row, which is the address of its first element. */
    first = *p;
    if (first[1] == 6 && (*p)[3] == 8) r = r + 1;

    /* Rows apart, not bytes apart. */
    q = grid + 2;
    if (q - p == 1 && q > p && p != q) r = r + 1;

    /* &a points at all of a: the same address, stepping all four ints. */
    if ((*whole)[2] == 30 && *whole == a && whole + 1 == &a + 1) r = r + 1;
    if (last_of(whole) == 40 && last_of(grid + 1) == 8) r = r + 1;

    /* Stores through the parenthesised form, and through subscripts. */
    (*whole)[0] = 99;
    (*(p + 1))[0] += 1;
    q[0][1]++;
    if (a[0] == 99 && grid[2][0] == 10 && grid[2][1] == 11) r = r + 1;

    /* 6 */
    return r + 36;
}
