/* expect: 3: error: a parameter's array size is evaluated when the function is entered, and acc cannot do that for one with a side effect */
int n;
int f(int a[n++]) { return a[0]; }
int main(void) { int b[2] = {0}; return f(b); }
