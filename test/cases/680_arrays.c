/* Arrays: local and global, of every type, with and without initial values,
 * indexed, stepped, passed to functions, and big enough that a local one
 * does not fit in the reach of (ix+d) -- with scalars declared after it that
 * still have to.
 */
int primes[6] = {2, 3, 5, 7, 11};       /* the sixth is zero */
char letters[] = {65, 66, 67};           /* three, counted */
long wide[3] = {100000, -2000000000};
float halves[4] = {0.5, 1.5, 2.5};
int zeros[50];
int *third = &primes[2];
int *all = primes;

int sum(int a[], int n) {
    int s = 0;

    for (int i = 0; i < n; i++)
        s += a[i];

    return s;
}

/* Through a pointer parameter, which an array argument becomes. */
int fill(int *a, int n, int v) {
    for (int i = 0; i < n; i++)
        a[i] = v + i;

    return n;
}

int *pick(void) {
    return primes;
}

/* Each call has an array of its own, below everything else in its frame. */
int depth(int n) {
    int mine[3];

    mine[0] = n;
    mine[2] = n * 2;
    if (n > 0)
        depth(n - 1);

    return mine[0] + mine[2];
}

/* Leaves its frame full of 77s, so that the next call at the same depth
 * starts on memory that is not zero -- which is how a missing zero fill
 * would show, rather than be hidden by a stack that happened to be clean. */
int dirty(void) {
    int junk[8];

    for (int i = 0; i < 8; i++)
        junk[i] = 77;

    return junk[7];
}

int clean(void) {
    int t[8] = {1};

    return t[1] + t[4] + t[7];
}

/* An array has to be inside its own frame, or the next call's frame lands
 * on it. */
int survives(void) {
    int keep[4] = {1, 2, 3, 4};

    dirty();

    return keep[0] + keep[1] + keep[2] + keep[3];
}

int main(void) {
    int r = 0;
    char buf[300];              /* past -128 wherever it goes */
    int local[5] = {9, 8};
    short shorts[4];
    long longs[] = {70000, 80000, 90000};
    float floats[2] = {1.25, 2.0 * 3};
    int *ptrs[3];
    int n = 3;                  /* declared after the big one */
    int x = 7;
    int *p;

    /* Globals, with the element no initialiser gave being zero. */
    if (sum(primes, 6) == 28 && primes[5] == 0) r = r + 1;
    if (letters[0] + letters[2] == 132) r = r + 1;
    if (wide[0] == 100000 && wide[1] == -2000000000 && wide[2] == 0) r = r + 1;
    if (halves[1] + halves[2] == 4.0 && halves[3] == 0.0) r = r + 1;
    if (*third == 5 && all[4] == 11) r = r + 1;

    /* A big local array, its last byte, and the scalars declared after it. */
    buf[0] = 1;
    buf[299] = 2;
    buf[n * 100 - 1] += 3;
    if (buf[0] + buf[299] == 6 && n == 3 && x == 7) r = r + 1;

    /* A local's initial values, and the zeros after them. */
    if (local[0] == 9 && local[1] == 8 && local[4] == 0) r = r + 1;
    if (sum(local, 5) == 17) r = r + 1;

    /* Written through the pointer an array argument becomes. */
    fill(local, 5, 10);
    if (local[0] == 10 && local[4] == 14) r = r + 1;
    fill(zeros + 10, 3, 1);
    if (zeros[10] == 1 && zeros[12] == 3 && zeros[13] == 0) r = r + 1;

    /* Narrow elements, which a store truncates and a load widens. */
    shorts[0] = 70000;
    shorts[1] = -5;
    if (shorts[0] == 4464 && shorts[1] == -5) r = r + 1;
    buf[1] = 200;
    if (buf[1] == -56) r = r + 1;

    /* Four-byte elements, sized by their initialiser. */
    longs[1] += 5;
    if (longs[0] + longs[1] + longs[2] == 240005) r = r + 1;
    floats[1] *= 2;
    if (floats[0] == 1.25 && floats[1] == 12.0) r = r + 1;

    /* Steps on an element, either side. */
    local[2] = 5;
    if (local[2]++ == 5 && local[2] == 6) r = r + 1;
    if (--local[2] == 5) r = r + 1;
    primes[0]++;
    if (primes[0] == 3) r = r + 1;

    /* An element's address, and an array used as a pointer. */
    p = &local[1];
    *p = 42;
    if (local[1] == 42 && *(local + 1) == 42 && p[1] == 5) r = r + 1;
    if (*local == 10 && *primes == 3) r = r + 1;

    /* An array of pointers, subscripted twice. */
    ptrs[0] = local;
    ptrs[1] = primes;
    ptrs[2] = zeros;
    if (ptrs[1][3] == 7 && ptrs[0][1] == 42) r = r + 1;
    ptrs[2][7] = 99;
    if (zeros[7] == 99) r = r + 1;

    /* Subscripts on things that are not names. */
    if ((local + 2)[0] == 5 && pick()[4] == 11) r = r + 1;

    /* Every call's array is its own. */
    if (depth(4) == 12) r = r + 1;

    /* The elements an initialiser leaves out are zero, whatever was there. */
    dirty();
    if (clean() == 0) r = r + 1;
    if (survives() == 10) r = r + 1;

    /* 25 */
    return r + 17;
}
