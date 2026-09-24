/* expect: belongs at the front of a declaration */
static struct { int static x; } s;
int main(void) { return 0; }
