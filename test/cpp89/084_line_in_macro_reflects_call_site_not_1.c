#define GET_LINE() __LINE__
int a = GET_LINE();
int b = GET_LINE();
int c = GET_LINE();
int check() { return (a == 2 && b == 3 && c == 4) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* __LINE__ in macro reflects call site (not 1)
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
