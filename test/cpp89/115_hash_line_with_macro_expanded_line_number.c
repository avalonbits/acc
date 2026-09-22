#define LINENUM 500
#line LINENUM
int x = __LINE__;
int check() { return (x == 500) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* #line with macro-expanded line number
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
