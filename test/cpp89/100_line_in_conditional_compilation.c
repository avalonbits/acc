#define MODE 1 // mode selection
int check() {
#if MODE == 1 // check mode
    return 0;
#else // other mode
    return 1;
#endif // end check
}

int main(void) { return check() == 0 ? 42 : 0; }

/* // in conditional compilation
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
