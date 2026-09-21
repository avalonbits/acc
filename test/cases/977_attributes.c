/* __attribute__, which acc reads and throws away.
 *
 * Nothing it can say changes the code acc emits: `noinline` and
 * `always_inline` are advice to an optimiser acc does not have, and the rest
 * of what a program writes is either advice too or something acc already
 * does. What matters is that the program still means what it meant with the
 * attribute taken out -- including where the attribute sits, which C lets be
 * almost anywhere a declaration is.
 *
 * agondev is clang, which acts on these. That is the point: the answer has
 * to be the same whether they were acted on or ignored. */

__attribute__((noinline))
static int noinlined(int n) { return n + 1; }

static inline __attribute__((always_inline)) int inlined(int n)
{
    return n + 2;
}

/* After the declarator, which is where a prototype carries one. */
static int later(int n) __attribute__((noinline));

static int later(int n) { return n + 3; }

/* More than one in a list, and more than one attribute. */
__attribute__((noinline, unused))
static int several(int n) { return n + 4; }

/* On a variable, and on a parameter. */
static int counted __attribute__((unused)) = 5;

static int with_param(int n __attribute__((unused)), int m)
{
    return m + 6;
}

int main(void)
{
    int total = 0;
    int unused_local __attribute__((unused)) = 0;

    total += noinlined(0);       /* 1 */
    total += inlined(0);         /* 2 */
    total += later(0);           /* 3 */
    total += several(0);         /* 4 */
    total += counted;            /* 5 */
    total += with_param(0, 0);   /* 6 */
    total += 21;

    return total;                /* 42 */
}
