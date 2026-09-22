#define MODE 2
int check() {
#if MODE == 1
    return 1;
#elif MODE == 2
    return 0;
#elif MODE == 3
    return 1;
#else
    return 1;
#endif
}

int main(void) { return check() == 0 ? 42 : 0; }

/* #elif selects correct branch
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
