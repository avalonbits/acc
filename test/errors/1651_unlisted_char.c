/* expect: 4:5: error: 'f' is declared with '()', which passes parameter 2 promoted, and with a type for it that is not */
/* A call through `()` promotes a char to an int, so the two disagree. */
int f();
int f(int a, char c);
int main(void) { return 0; }
