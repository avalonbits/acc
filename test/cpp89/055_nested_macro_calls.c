#define DOUBLE(x) ((x) * 2)
#define QUAD(x) DOUBLE(DOUBLE(x))
int check() { return (QUAD(3) == 12) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Nested macro calls
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
