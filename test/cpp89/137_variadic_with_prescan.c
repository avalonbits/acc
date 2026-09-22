#define A 10
#define B 20
#define ADD(x, ...) ((x) + __VA_ARGS__)
int check() { return (ADD(A, B) == 30) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Variadic with prescan
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
