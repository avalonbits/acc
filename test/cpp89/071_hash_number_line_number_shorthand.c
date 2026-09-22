# 200
int x = __LINE__;
int check() { return (x == 200) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* # number (line number shorthand)
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
