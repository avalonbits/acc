/* expect: 3:7: error: an array's size has to be a constant integer */
int n = 3;
int a[n];
int main(void) { return a[0]; }
