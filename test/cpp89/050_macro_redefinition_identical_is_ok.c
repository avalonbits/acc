#define DUP 42
#define DUP 42
int check() { return (DUP == 42) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Macro redefinition (identical) is OK
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
