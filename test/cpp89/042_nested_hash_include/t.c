#include "outer.h"
int check() { return (OUTER_VAL == 15) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Nested #include
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
