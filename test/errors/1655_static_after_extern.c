/* expect: 4:12: error: 'x' is declared static after a declaration that was not */
/* The block's extern gave x external linkage; C99 6.2.2p7. */
void f(void) { extern int x; }
static int x;
int main(void) { return 0; }
