#define STR(x) #x
#define XSTR(x) STR(x)
#define FOO hello
int check() {
    char *s = XSTR(FOO);
    return (s[0]=='h' && s[4]=='o' && s[5]==0) ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* XSTR/STR idiom: prescan before stringify
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
