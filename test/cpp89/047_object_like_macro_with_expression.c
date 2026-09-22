#define EXPR (2 + 3 * 4)
int check() { return (EXPR == 14) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Object-like macro with expression
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
