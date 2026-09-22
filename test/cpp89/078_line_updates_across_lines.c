

int a = __LINE__;
int check() { return (a == 3) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* __LINE__ updates across lines
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
