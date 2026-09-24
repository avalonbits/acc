/* expect: an array's size has to fit in an int */
int x[0x100000000LL];
int main(void) { return 0; }
