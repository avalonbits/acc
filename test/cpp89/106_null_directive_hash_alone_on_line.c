#
int check() { return 0; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Null directive (# alone on line)
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
