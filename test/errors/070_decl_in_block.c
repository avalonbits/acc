/* expect: 8:12: error: 'inner' is not declared */
/* A name declared in a block ends with the block. */
int main(void) {
    int n = 1;
    if (n) {
        int inner = 2;
    }
    return inner;
}
