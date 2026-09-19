/* A call to a function not yet declared adds a file-scope symbol, which
 * goes in below the function's own and moves them along. What is kept
 * beside each symbol has to move with it: a local array's count, which
 * sizeof and & read, came back as another symbol's. */
int main(void) {
    int r = 0;
    int a[5];
    char b[7];
    int n = later();
    int (*p)[5] = &a;

    if (sizeof a == 15 && sizeof b == 7) r++;
    if ((char *) (p + 1) - (char *) p == 15) r++;
    if (later() + n == 0) r++;

    return r + 39;          /* 3 checks */
}

int later(void) {
    return 0;
}
