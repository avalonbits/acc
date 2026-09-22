#define STR(x) #x
#define FOO hello
int check() {
    char *s = STR(FOO);
    return (s[0]=='F' && s[2]=='O' && s[3]==0) ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* # uses raw (unexpanded) argument
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
