/* #if and #elif on an expression, against agondev compiling the same
 * program.
 *
 * The evaluator is acc's own -- an #if is worked out in the widest integers
 * there are, with the macros in it expanded and `defined` answered before
 * anything else -- so what is worth asking another compiler is that both
 * decided the same branches. Each arm below leaves a different constant
 * behind, and only one of each can have been read.
 */
#define LEVEL   3
#define WIDE    (LEVEL * 4)

#if LEVEL > 5
enum { PICKED = 1 };
#elif LEVEL > 2 && defined(WIDE)
enum { PICKED = 20 };
#elif LEVEL > 0
enum { PICKED = 3 };
#else
enum { PICKED = 4 };
#endif

#if !defined(NOT_SET) && (WIDE == 12) && (0x10 == 16) && (010 == 8)
int step(int v) { return v + 1; }
#else
int step(int v) { return v - 1; }
#endif

#if NOT_DEFINED_AT_ALL
this need not be C
#endif

#if (1 << 4) == 16 ? 1 : 0
enum { SHIFTED = 21 };
#else
enum { SHIFTED = 0 };
#endif

int main(void) {
    return PICKED + SHIFTED + step(0);
}
