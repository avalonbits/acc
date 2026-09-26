/* expect: 8:7: error: '&&=' is not an operator in C; `a = a && b` is what it would mean */
/* C has a compound form of every binary operator except the two logical
 * ones. acc names the mistake rather than parsing `&&` and then tripping over
 * an `=` with nothing to its left. */
int main(void) {
    int a = 1;
    int b = 0;
    a &&= b;
    return a;
}
