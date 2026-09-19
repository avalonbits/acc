/* expect: 3: error: 'f' is declared again with another result type */
int f(int);
long f(int x) {
    return x;
}
int main(void) {
    return 0;
}
