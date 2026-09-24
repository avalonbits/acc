/* expect: belongs at the front of a declaration */
struct s { int static x; };
int main(void) { return 0; }
