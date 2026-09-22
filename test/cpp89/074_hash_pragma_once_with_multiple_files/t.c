#include "pa.h"
#include "pb.h"
#include "pa.h"
#include "pb.h"
int check() { return (PA_VAL + PB_VAL == 40) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* #pragma once with multiple files
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
