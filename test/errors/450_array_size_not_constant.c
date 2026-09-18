/* expect: 5: error: an array's size has to be a constant integer */
/* Variable-length arrays are C99, and not in acc yet. */
int main(void) {
    int n = 4;
    int a[n];
    return 0;
}
