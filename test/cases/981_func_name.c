/* __func__: the name of the function it is written in, as C99's
 *
 *     static const char __func__[] = "...";
 *
 * An array and not a pointer, so sizeof it is its length with the
 * terminator, and it is const, so it cannot be written through. */
static int same(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}

int first(void) { return __func__[0]; }

int marker_with_a_longer_name(void) { return (int) sizeof __func__; }

static int inner(void) {
    {
        const char *p = __func__;

        if (p[0] != 'i')
            return 0;
    }

    return same(__func__, "inner");
}

int main(void) {
    const char *p = __func__;
    int n = 0;

    if (!same(__func__, "main"))
        return 1;
    if (p[0] != 'm' || p[1] != 'a' || p[2] != 'i' || p[3] != 'n' || p[4] != 0)
        return 2;
    if (sizeof __func__ != 5)
        return 3;
    if (first() != 'f')
        return 4;
    if (marker_with_a_longer_name() != 26)
        return 5;
    if (!inner())
        return 6;

    /* Used twice in one function: the same array both times. */
    if (__func__ != p || &__func__[2] != p + 2)
        return 7;

    /* And as an ordinary array: indexed, walked, and passed. */
    while (__func__[n])
        n++;
    if (n != 4)
        return 8;

    return 42;
}
