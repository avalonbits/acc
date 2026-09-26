/* expect: 6:10: error: what a call comes to has no address to take */
int f(void) { return 1; }
int (*g(void))(void) { return f; }
int main(void) {
    int *p;
    p = &g()();
    return 0;
}
