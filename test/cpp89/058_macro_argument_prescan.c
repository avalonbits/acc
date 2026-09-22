#define X 1
#define ADD(a, b) ((a) + (b))
int check() { return (ADD(X, X) == 2) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Macro argument prescan
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
