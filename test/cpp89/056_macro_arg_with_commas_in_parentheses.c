#define FIRST(x) (x)
int f(int a, int b) { return a + b; }
int check() { return (FIRST(f(20, 22)) == 42) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Macro arg with commas in parentheses
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
