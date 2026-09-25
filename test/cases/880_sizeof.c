/* sizeof: of types, of expressions without evaluating them, of arrays whole
 * rather than as pointers, and of structs as agondev lays them out. */
struct s { char c; int i; long l; };
int later(void);                        /* defined at the end */
int g[10];
char text[] = "hello";

int calls;

int count(void) {
    calls++;

    return 1;
}

int main(void) {
    int r = 0;
    int a[4][5];
    int *p = g;
    int (*pa)[5] = a;
    int n = 0;
    struct s v, *pv = &v;

    if (sizeof(char) == 1 && sizeof(short) == 2 && sizeof(int) == 3) r++;
    if (sizeof(long) == 4 && sizeof(float) == 4 && sizeof(double) == 4) r++;
    if (sizeof(int *) == 3 && sizeof(char **) == 3) r++;
    if (sizeof(char *[5]) == 15 && sizeof(int (*)[5]) == 3) r++;
    if (sizeof g == 30 && sizeof p == 3 && sizeof(g) / sizeof(g[0]) == 10) r++;
    if (sizeof a == 60 && sizeof a[1] == 15 && sizeof a[1][2] == 3) r++;
    if (sizeof *pa == 15 && sizeof pa == 3) r++;
    if (sizeof text == 6 && sizeof "abc" == 4) r++;
    if (sizeof(count()) == 3 && sizeof n++ == 3 && calls == 0 && n == 0) r++;
    if (sizeof -text[0] == 3 && sizeof(text[0]) == 1 && sizeof(a[0][0] + 1L) == 4) r++;
    if (sizeof(int[7]) == 21) r++;
    if (sizeof(struct s) == 8 && sizeof v == 8 && sizeof *pv == 8) r++;
    if (sizeof pv->l == 4 && sizeof(v.c + 1) == 3 && sizeof v.c == 1) r++;

    /* What the operand compiled to is taken back: a call to a function not
     * yet defined, which would otherwise be patched later at an address that
     * now holds other code; a multiply, which calls into the runtime; and
     * values held in registers around it, which it may have moved. */
    n = 5;
    if (n * 2 + sizeof(later() * n) + n == 18 && calls == 0) r++;
    if (sizeof(a[n][n] / n) + (n - 1) * sizeof(char) == 7) r++;

    return r + 27;          /* 15 checks */
}

int later(void) {
    calls++;

    return 2;
}
