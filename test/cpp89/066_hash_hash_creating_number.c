#define PASTE(a, b) a ## b
int check() { return (PASTE(1, 0) == 10) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* ## creating number
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
