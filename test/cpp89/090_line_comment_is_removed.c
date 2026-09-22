int x = 1; // this is a comment
int check() { return (x == 1) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* // comment is removed
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
