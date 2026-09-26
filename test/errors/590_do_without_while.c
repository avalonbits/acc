/* expect: 5:12: error: expected 'while' after the body of a do, found 'return' */
/* A do's body is followed by its test. */
int main(void) {
    int n = 0;
    do n++;
    return n;
}
