/* expect: 3:23: error: 'x' is already declared */
/* A function's parameters and its outermost block are one scope. */
int f(double x) { int x = 1; return x; }
int main(void) { return f(2); }
