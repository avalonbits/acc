#define INNER() __LINE__
#define OUTER() INNER()
int a = OUTER();
int b = OUTER();
int check() { return (a == 3 && b == 4) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* __LINE__ in nested macros reflects call site
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
