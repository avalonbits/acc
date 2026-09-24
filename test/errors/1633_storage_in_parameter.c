/* expect: belongs at the front of a declaration */
int f(int static x) { return x; }
int main(void) { return f(0); }
