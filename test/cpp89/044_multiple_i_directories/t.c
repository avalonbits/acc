#include "hdr1.h"
#include "hdr2.h"
int check() { return (V1 + V2 == 30) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* Multiple -I directories
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
