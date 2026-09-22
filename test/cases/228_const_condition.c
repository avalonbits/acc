/* `&&`, `||` and `?:` worked out while compiling.
 *
 * C says all three make a constant expression when their operands are
 * constants, and acc folded none of them: `1 && 1` was refused as an initial
 * value, and `?:` was not accepted in an array's size at all. Its own header
 * asserts at build time with an array of `cond ? 1 : -1`, which is why it
 * could not compile itself.
 *
 * The side a constant condition does not pick is not evaluated, and the code
 * that worked it out is taken back -- so the cases below put something with
 * an effect on the side that loses and check the effect did not happen.
 */
static int calls;
static int bump(void) { calls++; return 1; }
static int id(int x) { return x; }

/* Array sizes, which is where the folding has to reach for the assertion
 * idiom to work at all. */
static char sized_by_and[(1 == 1 && 2 == 2) ? 4 : -1];
static char sized_by_or[(0 || 1) ? 3 : -1];
static char sized_by_cond[1 ? 2 : -1];
static int table[1 && 1];

enum { FROM_AND = 1 && 1, FROM_OR = 0 || 7, FROM_COND = 1 ? 9 : 8 };

static int g_and = 1 && 1;
static int g_or = 0 || 1;
static int g_cond = 1 ? 5 : 6;
static int g_mixed = (1 && 0) ? 11 : 12;

int main(void)
{
    if (sizeof sized_by_and != 4) return 1;
    if (sizeof sized_by_or != 3) return 2;
    if (sizeof sized_by_cond != 2) return 3;
    if (sizeof table != sizeof(int)) return 4;

    if (FROM_AND != 1 || FROM_OR != 1 || FROM_COND != 9) return 5;
    if (g_and != 1 || g_or != 1 || g_cond != 5 || g_mixed != 12) return 6;

    /* The side that loses is not evaluated. */
    calls = 0;
    if ((0 && bump()) != 0) return 7;
    if (calls != 0) return 8;
    if ((1 || bump()) != 1) return 9;
    if (calls != 0) return 10;
    if ((1 ? 1 : bump()) != 1) return 11;
    if (calls != 0) return 12;
    if ((0 ? bump() : 2) != 2) return 13;
    if (calls != 0) return 14;

    /* The side that wins is. */
    if ((1 && bump()) != 1) return 15;
    if (calls != 1) return 16;
    if ((0 || bump()) != 1) return 17;
    if (calls != 2) return 18;

    /* A constant on the left that does not settle it: the answer is the
     * right as a one or a nought, with no branch. */
    if ((1 && id(0)) != 0) return 19;
    if ((1 && id(7)) != 1) return 20;
    if ((0 || id(0)) != 0) return 21;
    if ((0 || id(7)) != 1) return 22;

    /* Nothing constant at all still works the old way. */
    if ((id(1) && id(2)) != 1) return 23;
    if ((id(0) && id(2)) != 0) return 24;
    if ((id(0) || id(3)) != 1) return 25;
    if ((id(1) ? id(8) : id(9)) != 8) return 26;
    if ((id(0) ? id(8) : id(9)) != 9) return 27;

    return 42;
}
