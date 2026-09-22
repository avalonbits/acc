#define VAR(n) var_ ## n
int check() {
    int var_1 = 10, var_2 = 20;
    return (VAR(1) + VAR(2) == 30) ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* ## creating identifier from parts
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
