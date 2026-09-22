int check() {
#if (0xFF & 0x0F) == 0x0F && (0xF0 | 0x0F) == 0xFF
    return 0;
#else
    return 1;
#endif
}

int main(void) { return check() == 0 ? 42 : 0; }

/* #if bitwise operators (& | ^ ~)
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
