#define A 1
#define B 1
int check() {
#if defined(A) && defined(B) && !defined(C)
    return 0;
#else
    return 1;
#endif
}

int main(void) { return check() == 0 ? 42 : 0; }

/* defined in compound expression
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
