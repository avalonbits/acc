int a = __LINE__;
int b = __LINE__;
int check() { return (a == 1 && b == 2) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* __LINE__ is current line number
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
