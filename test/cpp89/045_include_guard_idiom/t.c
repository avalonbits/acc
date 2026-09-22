#include "guarded.h"
#include "guarded.h"
int check() { return (GUARDED_VAL == 1) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Include guard idiom
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
