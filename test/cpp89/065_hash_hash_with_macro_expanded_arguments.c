#define A 1
#define B 2
#define PASTE(x, y) x ## y
int check() {
    int ab = 42;
    return (PASTE(a, b) == 42) ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* ## with macro-expanded arguments
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
