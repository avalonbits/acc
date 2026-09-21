/* A directive continued onto the next line with a backslash.
 *
 * A macro's body has been joined this way since there were macros. The
 * condition of an #if was not: it was read as the rest of one line, so a
 * `#if defined(A) || defined(B) \` stopped at the backslash, which was left
 * in the text and refused as something over at the end of the condition.
 * zap writes its cut-down builds that way. */

#define WIDE(a, b) \
    ((a) * 10 +    \
     (b))

#define ONE   1
#define THREE 3

#if defined(ONE) && ONE == 1 \
    && !defined(TWO)         \
    && THREE == 3
#define JOINED 4
#else
#define JOINED 0
#endif

/* The false side too: a continued condition that is not taken, and the
 * group under it skipped. */
#if defined(TWO) \
    || defined(FOUR)
#define TAKEN_WRONGLY 1
int wrong(void) { return 1; }
#endif

#if JOINED == 4 \
    && !defined(TAKEN_WRONGLY)
#define RIGHT 5
#endif

int main(void)
{
    int total = 0;

    total += WIDE(3, 3);         /* 33 */
    total += JOINED;             /* 4 */
    total += RIGHT;              /* 5 */

    return total;                /* 42 */
}
