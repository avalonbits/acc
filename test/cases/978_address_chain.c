/* `&` applied to something that is not a name: a dereference, and what
 * follows a parenthesis round one.
 *
 * `&*p` is p, and `&(*p)[i]` is `*p + i` -- the `&` and the `*` cancel, and
 * the subscript is the one piece of arithmetic left. acc used to refuse both
 * and say the `*` was not a variable, which is how `&(*list)[n]` -- a fixup
 * table grown through a pointer to it -- could not be written. */

static int numbers[6] = { 10, 20, 30, 40, 50, 60 };

struct pair { int x, y; };

static struct pair pairs[3] = { { 1, 2 }, { 3, 4 }, { 5, 6 } };

/* What zap does with it: a table the caller owns, reached through a pointer
 * to the pointer, with the next slot's address taken. */
static int *slot(int **table, int n)
{
    return &(*table)[n];
}

int main(void)
{
    int *p = numbers;
    int **pp = &p;
    struct pair *s = &pairs[1];
    struct pair **ss = &s;
    int total = 0;

    total += *&*p;               /* 10: the two cancel */
    total += *&(*pp)[2];         /* 30 */
    total += *slot(pp, 4);       /* 50 */
    total += (&(*pp)[3]) - p;    /* 3: an index, not an address */
    total += (*ss)->y;           /* 4 */
    total += *&(*s).x;           /* 3 */
    total += *&(numbers)[5];     /* 60: a whole array, subscripted */

    /* Written through, to show the address was the element's own. */
    *&(*pp)[0] = 1;
    total += numbers[0];         /* 1 */

    return total - 119;          /* 42 */
}
