/* Declarations that agree, which acc has to keep taking now that it asks:
 * - `()` and a list whose parameters a call through `()` passes as they
 *   are -- an int, a pointer, a long (C99 6.7.5.3p15);
 * - `static` and then `extern`, or a function with no storage class, which
 *   take the linkage already given (6.2.2p4, p5);
 * - `(void)` and a definition with `()`, both with no parameters. */
int add();
int add(int a, long b, char *p);

static int base;
extern int base;

static int twice(int n);
int twice(int n);

int none(void);
int none() { return 2; }

int add(int a, long b, char *p) { return a + (int) b + *p; }

static int twice(int n) { return n * 2; }

int main(void)
{
    char c = 10;

    base = 5;

    return add(base, 15L, &c) + twice(none()) + 8;
}
