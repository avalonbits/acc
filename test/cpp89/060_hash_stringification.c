#define STR(x) #x
char *s = STR(hello);
int check() {
    return (s[0]=='h' && s[1]=='e' && s[4]=='o' && s[5]==0) ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* # (stringification)
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
