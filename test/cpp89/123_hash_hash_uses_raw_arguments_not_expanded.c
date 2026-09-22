#define PASTE(a, b) a ## b
#define X y
int check() {
    int Xy = 42;
    return (PASTE(X, y) == 42) ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* ## uses raw arguments (not expanded)
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
