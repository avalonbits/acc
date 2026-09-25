/* expect: 3: error: 'f' is defined with no parameters, and was declared with 1 */
int f(int n);
int f() { return 0; }
int main(void) { return f(1); }
