#define SVAR(...) #__VA_ARGS__
int check() {
    char *s = SVAR(hello, world);
    return (s[0]=='h' && s[5]==',') ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* Stringify __VA_ARGS__
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
