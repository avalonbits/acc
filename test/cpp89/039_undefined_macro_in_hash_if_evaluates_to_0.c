int check() {
#if UNDEFINED_THING
    return 1;
#else
    return 0;
#endif
}

int main(void) { return check() == 0 ? 42 : 0; }

/* Undefined macro in #if evaluates to 0
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
