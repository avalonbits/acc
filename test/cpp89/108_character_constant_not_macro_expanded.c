#define A 0
int check() { return ('A' == 65) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Character constant not macro-expanded
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
