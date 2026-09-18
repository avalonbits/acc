/* expect: 7: error: '++' is not supported yet */
/* `++` is lexed although it is not implemented. Left as two pluses it was not
 * refused at all: `a ++ b` read as `a + +b` and compiled without a word. */
int main(void) {
    int a = 1;
    int b = 2;
    return a ++ b;
}
