int check() {
#ifdef NOPE
    return 1;
#else
    return 0;
#endif
}

int main(void) { return check() == 0 ? 42 : 0; }

/* #ifdef false
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
