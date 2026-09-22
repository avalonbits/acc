#define ADD(a, b) ((a) + (b))
#define X 10
#define Y 20
int check() { return (ADD(X, Y) == 30) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Prescan with multiple macro args
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
