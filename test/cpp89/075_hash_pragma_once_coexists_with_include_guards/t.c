#include "both.h"
#include "both.h"
int check() { return (BOTH_VAL == 55) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* #pragma once coexists with include guards
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
