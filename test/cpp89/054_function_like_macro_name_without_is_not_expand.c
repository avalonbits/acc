#define FOO(x) ((x) * 2)
int check() {
    int FOO = 5;
    return (FOO == 5) ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* Function-like macro name without () is not expanded
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
