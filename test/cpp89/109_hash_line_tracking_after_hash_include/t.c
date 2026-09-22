int before = __LINE__;
#include "one_line.h"
int after = __LINE__;
int check() { return (before == 1 && after == 3) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* #line tracking after #include
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
