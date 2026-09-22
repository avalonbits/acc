    #define INDENTED 1
int check() { return (INDENTED == 1) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Leading whitespace before #
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
