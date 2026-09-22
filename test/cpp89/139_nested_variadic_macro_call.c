#define INNER(...) __VA_ARGS__
#define OUTER(...) INNER(__VA_ARGS__)
int add(int a, int b) { return a + b; }
int check() { return (OUTER(add(20, 22)) == 42) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Nested variadic macro call
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
