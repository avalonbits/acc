/* expect: what is in the parenthesis is a value */
int main(void) { int a = 1; int *p = &(a + 1); return *p; }
