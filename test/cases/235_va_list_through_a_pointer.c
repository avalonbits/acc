/* A va_list reached through something other than its own name.
 *
 * C99 says va_arg and the rest take the va_list object, and a function that
 * walks arguments on another's behalf is handed a pointer to it -- which is
 * the one way to have the walking show up in the caller, since a va_list
 * passed by value is a copy. So `va_arg(*ap, T)` is ordinary C, and acc
 * asked for a bare name and refused everything else. Five of the gcc
 * torture tests are this.
 *
 * Where the object is, is what `&` asks of an operand, so these ask it the
 * same way: a name, a star, a subscript, a parenthesis.
 */
#include <stdarg.h>

/* Walks two ints off the caller's list. What it steps over has to be gone
 * when it comes back, which is the whole point of the pointer. */
static int two(va_list *ap) {
    int a = va_arg(*ap, int);
    int b = va_arg(*ap, int);

    return a * 10 + b;
}

static int through_a_pointer(int n, ...) {
    va_list ap;
    va_list *p = &ap;
    int first, rest, last;

    va_start(*p, n);
    first = va_arg(*p, int);
    rest = two(&ap);                    /* moves ap along, not a copy of it */
    last = va_arg(ap, int);
    va_end(*p);

    return first * 1000 + rest * 10 + last;
}

/* A va_list in an array, subscripted: the same question with a bracket. */
static int through_a_subscript(int n, ...) {
    va_list aps[2];
    int a, b;

    va_start(aps[0], n);
    a = va_arg(aps[0], int);
    va_copy(aps[1], aps[0]);            /* both sit where a is not */
    b = va_arg(aps[1], int) + va_arg(aps[0], int);
    va_end(aps[1]);
    va_end(aps[0]);

    return a * 100 + b;
}

/* A copy taken through pointers to both, which has to walk on its own. */
static int copied_through_pointers(int n, ...) {
    va_list ap, copy;
    va_list *from = &ap, *to = &copy;
    int a, b, c;

    va_start(*from, n);
    a = va_arg(*from, int);
    va_copy(*to, *from);
    b = va_arg(*to, int);
    c = va_arg(*from, int);
    va_end(*to);
    va_end((ap));                       /* a parenthesis says nothing */

    return a * 100 + b * 10 + c;
}

int main(void) {
    int r = 0;

    if (through_a_pointer(4, 1, 2, 3, 4) == 1234) r++;
    if (through_a_subscript(3, 5, 6, 7) == 512) r++;
    if (copied_through_pointers(3, 1, 2, 2) == 122) r++;

    return r + 39;              /* 3 checks */
}
