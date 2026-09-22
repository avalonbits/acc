int a = 10; // comment
int b = 20;
int check() { return (a + b == 30) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* // does not consume next line
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
