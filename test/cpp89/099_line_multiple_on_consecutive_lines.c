// line 1
// line 2
// line 3
int check() { return 0; }

int main(void) { return check() == 0 ? 42 : 0; }

/* // multiple on consecutive lines
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
