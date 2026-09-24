/* expect: a member's size has to be known */
int main(void) { int n = 3; struct { int a[n]; } s; return 0; }
