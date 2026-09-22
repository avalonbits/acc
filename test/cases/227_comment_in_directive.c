/* Comments where a directive is, including ones that outlast the line.
 *
 * C takes comments out before it reads directives, and replaces each with a
 * space. So a comment that opens on a `#define` and closes two lines down is
 * still part of that define, and what follows the close is still part of it
 * too -- the directive ends at the first newline that is not inside a
 * comment, not at the first newline.
 *
 * acc read a directive's line as text. The body kept the opening of the
 * comment, and the lines beneath it were compiled as though they were code:
 * `expected a type, found '*'`, pointing at the middle of a comment. Its own
 * gen.c has a define laid out that way, which is how this was found.
 */
#define ONE 1               /* on one line */
#define TWO 2               /* laid out
                             * over two */
#define THREE(x) ((x) + /* in the middle
                         * of the body */ 1)
#define FOUR 4              // to the end of the line
#define FIVE 2 /* before */ + /* between */ 3 /* and after */

/* One in the condition of an #if, which is read the same way. */
#if TWO == 2                /* still the same directive
                             * down here */
#define SIX 6
#else
#define SIX 0
#endif

/* And one that ends a define with nothing after it, so the body is the
 * comment and a space is all that is left. */
#define SEVEN 7 /* trailing
                 */

int main(void)
{
    if (ONE != 1) return 1;
    if (TWO != 2) return 2;
    if (THREE(10) != 11) return 3;
    if (FOUR != 4) return 4;
    if (FIVE != 5) return 5;
    if (SIX != 6) return 6;
    if (SEVEN != 7) return 7;

    /* The pieces either side of a comment join with a space between them
     * and not with nothing, so FIVE is 2 + 3 and not 23. */
    if (FIVE != 2 + 3) return 8;

    return 42;
}
