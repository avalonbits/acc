#if 0
#error This should NOT fire
#endif
int check() { return 0; }

int main(void) { return check() == 0 ? 42 : 0; }

/* #error only in taken branch
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
