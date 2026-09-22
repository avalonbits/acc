#define FIRST(a, ...) a
int check() { return (FIRST(0) == 0) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Variadic with empty __VA_ARGS__
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
