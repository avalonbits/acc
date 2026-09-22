#define FIRST_OF(fmt, ...) fmt
#define ADD3(f, ...) f(__VA_ARGS__)
int sum(int a, int b) { return a + b; }
int check() {
    return (FIRST_OF(0, 1, 2) == 0
         && ADD3(sum, 20, 22) == 42) ? 0 : 1;
}

int main(void) { return check() == 0 ? 42 : 0; }

/* Basic variadic macro
 *
 * From Decus CPP's C89 conformance suite, which is in the public
 * domain. Its programs answer 0; the last line above is acc's 42. */
