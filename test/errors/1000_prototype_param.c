/* expect: 3: error: 'f' is declared again with parameter 1 of another type */
int f(char);
int f(int a) {
    return a;
}
int main(void) {
    return 0;
}
