/* expect: belongs at the front of a declaration */
static enum { A = (int static) 1 } e;
int main(void) { return 0; }
