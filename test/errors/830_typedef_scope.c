/* expect: 7: error: 'wide' is not declared */
/* A typedef in a block ends with the block, as a variable does. */
int main(void) {
    {
        typedef long wide;
    }
    wide w = 1;
    return 0;
}
