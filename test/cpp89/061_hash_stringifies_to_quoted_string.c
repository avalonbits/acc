#define STR(x) #x
char *s = STR(test);
int check() { return (s[0] == 't') ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* # stringifies to quoted string
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
