/* What const protects beyond a variable itself -- a pointer's target, an
 * array's elements, a struct's members -- read freely, and written only
 * where a cast or a pointer that is not to const says it may be. */
struct point { const int id; int x; };
typedef const char cchar;

const int table[3] = { 1, 2, 3 };
const struct point origin = { 7, 0 };

const int *pick(int i) {
    return &table[i];
}

int total(const int *p, int n) {
    int s = 0;

    while (n--)
        s += *p++;

    return s;
}

int main(void) {
    int r = 0;
    int cells[3] = { 4, 5, 6 };
    const int *cp = cells;
    int *wp = (int *) cp;
    const int **cpp = &cp;
    struct point p = { 1, 2 };
    cchar *name = "abc";
    char buf[2];
    char *out = buf;

    if (total(table, 3) == 6 && *pick(2) == 3) r++;
    *wp = 40;                       /* not through the pointer to const */
    if (cp[0] == 40 && **cpp == 40) r++;
    cp = table;                     /* the pointer itself is not const */
    *cpp = cells + 1;
    if (*cp == 5) r++;
    p.x = 9;                        /* the member that is not const */
    if (p.id + p.x == 10 && origin.id == 7) r++;
    *out = name[1];
    if (buf[0] == 'b') r++;

    return r + 37;          /* 5 checks */
}
