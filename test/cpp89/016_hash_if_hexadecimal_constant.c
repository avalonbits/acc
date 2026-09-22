int check() {
#if 0x1A == 26
    return 0;
#else
    return 1;
#endif
}

int main(void) { return check() == 0 ? 42 : 0; }

/* #if hexadecimal constant
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
