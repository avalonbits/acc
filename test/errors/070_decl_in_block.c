/* expect: 5: error: a declaration has to be at the start of the function */
int main(void) {
    int n = 1;
    if (n) {
        int inner = 2;
        n = inner;
    }
    return n;
}
