int check() {
#if 1
#if 1
#if 1
#if 1
#if 1
    return 0;
#endif
#endif
#endif
#endif
#endif
}

int main(void) { return check() == 0 ? 42 : 0; }

/* Deeply nested conditionals
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
