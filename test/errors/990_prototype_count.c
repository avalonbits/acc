/* expect: 3:5: error: 'f' is declared again with 2 parameters, not 1 */
int f(int);
int f(int a, int b) {
    return a + b;
}
int main(void) {
    return 0;
}
