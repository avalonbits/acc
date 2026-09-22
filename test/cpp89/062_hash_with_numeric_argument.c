#define STR(x) #x
char *s = STR(42);
int check() { return (s[0] == '4' && s[1] == '2' && s[2] == 0) ? 0 : 1; }

int main(void) { return check() == 0 ? 42 : 0; }

/* # with numeric argument
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
