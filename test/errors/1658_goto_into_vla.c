/* expect: 4:5: error: this goto jumps past the declaration of an array whose length is worked out as it runs, into its scope */
/* C99 6.8.6.1p1: the array would exist without its length worked out. */
int f(int n) {
    goto in;
    {
        int a[n];
    in:
        a[0] = 1;
        return a[0];
    }
}
int main(void) { return f(2); }
