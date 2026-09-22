#define STR(x) #x
#define XSTR(x) STR(x)
#define ADD(a,b) a + b
int check() {
    char *s = XSTR(ADD(1,2));
    return (s[0]=='1' && s[2]=='+' && s[4]=='2') ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* XSTR of function-like macro expansion
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
