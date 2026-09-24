/* expect: a static or extern array's length has to be known */
int main(void) { int n = 3; static int a[2][n]; return 0; }
