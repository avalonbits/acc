/* expect: 3:5: error: 'f' is declared again without '...' */
int f(int a, ...);
int f(int a) {
    return a;
}
int main(void) {
    return 0;
}
