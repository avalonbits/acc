/* A loop that runs, and one whose condition is false to begin with, which
   must not run its body at all. */
int main(void) {
    int n = 8;
    int r = 0;
    int never = 0;

    while (n) {
        r = r + n;
        n = n - 1;
    }
    /* 8+7+6+5+4+3+2+1 = 36 */

    while (never)
        r = r + 1000;

    return r + 6;
}
