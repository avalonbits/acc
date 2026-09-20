/* #define and #undef, against agondev compiling the same program.
 *
 * What a macro is worth checking for here is that it produces the same
 * tokens the text would have: a constant used where a constant goes, an
 * expression that keeps its parentheses, a name standing for a type, and
 * one whose definition is several tokens long. The cases that need no
 * second compiler -- what is refused, and a macro that stands for itself --
 * are in test/macro.sh.
 */
#define WIDTH       6
#define HEIGHT      7
#define AREA        (WIDTH * HEIGHT)
#define COUNTER     unsigned int
#define ADD_ONE     total = total + 1;
#define EMPTY

int main(void) {
    COUNTER total = 0;
    int i;

    for (i = 0; i < WIDTH; i++)
        ADD_ONE

#undef WIDTH
#define WIDTH 100

    EMPTY

    /* AREA was written with the old WIDTH but is expanded now, so it is
     * 100 * 7 and not 6 * 7: a macro is text, and the text is read where
     * it is used. */
    if (AREA != 700)
        return 1;

    return (int) total + 36;
}
