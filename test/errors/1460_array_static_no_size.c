/* expect: 2:12: error: 'static' inside [] needs the number the caller passes at least */
int f(int a[static]) { return a[0]; }
int main(void) { int a[2]; a[0] = 1; return f(a); }
