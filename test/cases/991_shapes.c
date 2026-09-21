/* Shapes of C that real programs are written in and that acc had not met.
 *
 * Every one of these came out of compiling a text editor of sixteen thousand
 * lines: a parenthesis round the left of an assignment, the address of a
 * parenthesised object, the same typedef said twice by two headers that
 * refer to each other, a void pointer compared with a real one, a macro
 * written over several lines, and a claim about a constant checked as it
 * compiles. None of them is unusual; all of them were refused.
 */
/* The cases are compiled with nothing to include, so this is spelled out
 * here as <stddef.h> spells it. */
#define NULL ((void *) 0)

/* Two headers that name each other's types do this: one says the name and
 * the other completes it. */
struct _box;
typedef struct _box box;
typedef struct _box {
    int n;
    int m;
} box;

_Static_assert(sizeof(box) == 6, "two ints on this machine come to six bytes");

/* A macro over more than one line, which is how any macro worth having is
 * written. */
#define SPREAD(a, b) \
    ( (a) > (b)      \
        ? (a) - (b)  \
        : (b) - (a) )

static int twice(int n)         /* static: this file's alone */
{
    return n + n;
}

box the_box;
int number = 20;
int table[4];

int main(void)
{
    int *p = &number;
    void *v = table;
    int r = 0;

    (*p) += 1;                          /* a parenthesis round the left */
    if (number == 21) r++;
    (*p) = 4;
    if (number == 4) r++;

    if (&(number) == p && &(table[2]) == table + 2) r++;
    if (&(the_box.m) == &the_box.m) r++;

    if (v == table && v != NULL && p != NULL) r++;

    if (SPREAD(3, 10) == 7 && SPREAD(10, 3) == 7) r++;

    /* A sizeof is unsigned, and comparing one with a number is a constant
     * the compiler can work out for itself. */
    if (sizeof(box) == 6 && sizeof(table) > 4) r++;

    if (twice(number) == 8) r++;

    return r + 34;                      /* 8 checks */
}
