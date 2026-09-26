/* expect: 3:11: error: 'f' is declared again with another result type */
int f(void);
const int f(void) { return 1; }
int main(void) { return f(); }
