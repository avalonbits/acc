#include "check_line.h"
int check() { return (inc_line == 1) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* __LINE__ in #include file reflects include file
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
