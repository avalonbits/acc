/* expect: a static or extern array's length has to be known */
int main(void) { int n = 3; extern int a[n]; return 0; }
