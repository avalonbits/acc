/* expect: 3: error: 'f' is declared with '()' and with '...', which do not agree */
int f(int n, ...);
int f();
int main(void) { return 0; }
