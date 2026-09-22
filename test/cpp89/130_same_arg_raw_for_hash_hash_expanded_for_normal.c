#define MIX(a, b) a ## b + a
#define X 10
int check() {
    int X0 = 5;
    return (MIX(X, 0) == 15) ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* Same arg: raw for ##, expanded for normal
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
