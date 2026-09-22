#include "custom.h"
int check() { return (CUSTOM == 123) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* #include with -I directory search
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
