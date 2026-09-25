/* expect: 3: error: 'u' is declared static after a declaration that was not */
int u(void);
static int u(void) { return 0; }
int main(void) { return u(); }
