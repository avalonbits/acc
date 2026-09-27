/* expect: 6:5: error: the switch would jump to this label past the declaration of an array whose length is worked out as it runs, into its scope */
/* C99 6.8.4.2p2. */
int f(int k, int n) {
    switch (k) {
        int a[n];
    case 1:
        return 1;
    }
    return 0;
}
int main(void) { return f(1, 2); }
