#define ID(x) x
int check() {
    char *s = ID("hello, world");
    return (s[0] == 'h') ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* Macro arg containing string with comma
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
