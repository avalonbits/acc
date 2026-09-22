int check() {
#if 1
    return 0;
#elif THIS_WOULD_BE_AN_ERROR
    return 1;
#endif
}

int main(void) { return check() == 0 ? 42 : 0; }

/* #elif not evaluated after true #if
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
