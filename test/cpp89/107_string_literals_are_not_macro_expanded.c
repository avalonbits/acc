#define VALUE 42
char *s = "VALUE";
int check() { return (s[0]=='V' && s[4]=='E' && s[5]==0) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* String literals are not macro-expanded
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
