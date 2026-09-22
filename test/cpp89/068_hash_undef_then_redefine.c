#define VAL 1
#undef VAL
#define VAL 2
int check() { return (VAL == 2) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* #undef then redefine
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
