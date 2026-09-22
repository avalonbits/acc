#define DOUBLE(x) ((x) + (x))
int check() { return (DOUBLE(21) == 42) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Function-like macro, one arg
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
