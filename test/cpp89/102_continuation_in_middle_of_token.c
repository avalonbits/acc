#define AB\
CD 42
int check() { return (ABCD == 42) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Continuation in middle of token
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
