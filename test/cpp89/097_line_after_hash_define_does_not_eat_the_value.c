#define A 1 // first
#define B 2 // second
int check() { return (A + B == 3) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* // after #define does not eat the value
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
