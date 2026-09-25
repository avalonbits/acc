/* expect: 10: error: this goto jumps past the declaration of an array whose length is worked out as it runs, into its scope */
/* Backwards too, and a pointer to such an array counts. */
int f(int n) {
    int k = 0;
    {
        int (*p)[n] = 0;
    in:
        if (k++) return p != 0;
    }
    goto in;
}
int main(void) { return f(2); }
