#define EMPTY
int EMPTY check() { return 0; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Macro defined to empty string
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
