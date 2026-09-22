#define CALL(fn, ...) fn(__VA_ARGS__)
int add(int a, int b, int c) { return a + b + c; }
int check() { return (CALL(add, 10, 20, 12) == 42) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Variadic collects multiple args as one
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
