/* Arithmetic on the address of the very first thing in the image.
 *
 * A value acc has not had to emit yet carries a number in `val`, and for an
 * address that number is where the thing will be rather than what it is
 * worth. The first object in the bss is at offset zero, so an address
 * operand and the integer nought looked exactly alike -- and `p - first`
 * took the path that says adding or subtracting nothing is nothing, and
 * compiled to no subtraction at all. It came out as p divided by the width
 * of the element, which for the sizes below is not even close.
 *
 * So: the first array declared in this file, used on both sides of both
 * operators, at a few element widths. `first` has to stay first for this to
 * be testing anything -- anything declared above it takes offset zero
 * instead and the bug hides again.
 *
 * `second` is here to show the difference: it is the same in every way
 * except that something is in front of it.
 */
struct wide { char pad[13]; };

static struct wide first[4];            /* offset zero: the one that broke */
static struct wide second[4];
static int ints[4];
static char chars[4];

/* An initialised object goes in the image rather than the bss, and the first
 * of those is at offset zero of its own. */
static char text[4] = { 1, 2, 3, 4 };

static int opaque(int x) { return x; }

int main(void)
{
    struct wide *pf = first + 1;
    struct wide *ps = second + 1;
    int *pi = ints + 1;
    char *pc = chars + 1;
    char *pt = text + 1;

    /* Pointer minus array name, which is what compiled to nothing. */
    if (pf - first != 1) return 1;
    if (ps - second != 1) return 2;
    if (pi - ints != 1) return 3;
    if (pc - chars != 1) return 4;
    if (pt - text != 1) return 5;

    /* The same distance the other way round. */
    if (first - pf != -1) return 6;
    if (text - pt != -1) return 7;

    /* Adding to the address, where the operand is on either side. */
    if (first + 1 != pf) return 8;
    if (1 + first != pf) return 9;
    if (chars + 1 != pc) return 10;

    /* Through a value the compiler cannot fold away, so the arithmetic has
     * to be done rather than worked out while compiling. */
    if (first + opaque(2) != first + 2) return 11;
    if ((first + opaque(3)) - first != 3) return 12;

    /* The bytes of the initialised one are still where they should be. */
    if (text[0] != 1 || text[3] != 4) return 13;
    if (*(text + opaque(2)) != 3) return 14;

    /* And the addresses really are distinct: if `first` were nought the
     * differences above could agree by accident. */
    if ((char *) first == (char *) 0) return 15;
    if (first == second) return 16;

    return 42;
}
