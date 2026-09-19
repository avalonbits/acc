/* long double: eight bytes of IEEE 754 double, which is what agondev makes
 * it and nearly all it makes it. libagon converts one to and from the
 * integers and does nothing else with it -- no add, no compare, not even a
 * conversion to or from a float -- so what acc does with one is hold it,
 * move it, pass it, return it and convert it, and that is what is here.
 *
 * The checks are on the bytes rather than on what a conversion back makes
 * of them, because agondev's own conversion from a double to a long is
 * wrong: it answers 0 for 1.5L, 2.0L and 1e9L alike. The other direction
 * agrees, and is checked. acc's own is checked in test/self, where there is
 * no second compiler to agree with. */
long double g = 1.5L;
long double table[3] = { 0.5L, -2.5L, 1e18L };

struct held { char tag; long double v; };

long double widen(long n) { return n; }
long double copy(long double v) { return v; }
long double pick(int which, long double a, long double b) { return which ? a : b; }

/* The eight bytes of v, against the eight given. */
static int same(long double v, unsigned char b7, unsigned char b6,
                unsigned char b5, unsigned char b4, unsigned char b3,
                unsigned char b2, unsigned char b1, unsigned char b0) {
    unsigned char *p = (unsigned char *) &v;

    return p[7] == b7 && p[6] == b6 && p[5] == b5 && p[4] == b4
        && p[3] == b3 && p[2] == b2 && p[1] == b1 && p[0] == b0;
}

int main(void) {
    long double x = 3.25L, y;
    struct held h;
    long double row[2];

    if (sizeof(long double) != 8 || sizeof table != 24 || sizeof(struct held) != 9)
        return 1;
    if (!same(x, 0x40, 0x0a, 0, 0, 0, 0, 0, 0))         /* 3.25 */
        return 2;
    y = x;
    if (!same(y, 0x40, 0x0a, 0, 0, 0, 0, 0, 0))
        return 3;
    if (!same(g, 0x3f, 0xf8, 0, 0, 0, 0, 0, 0))         /* 1.5 */
        return 4;
    if (!same(table[0], 0x3f, 0xe0, 0, 0, 0, 0, 0, 0))  /* 0.5 */
        return 5;
    if (!same(table[1], 0xc0, 0x04, 0, 0, 0, 0, 0, 0))  /* -2.5 */
        return 6;
    if (!same(table[2], 0x43, 0xab, 0xc1, 0x6d, 0x67, 0x4e, 0xc8, 0))  /* 1e18 */
        return 7;
    if (!same(-1.5L, 0xbf, 0xf8, 0, 0, 0, 0, 0, 0))
        return 8;
    if (!same(0.1L, 0x3f, 0xb9, 0x99, 0x99, 0x99, 0x99, 0x99, 0x9a))
        return 9;

    /* An integer converted to one, which is the direction agondev's library
     * gets right. */
    if (!same(widen(1L), 0x3f, 0xf0, 0, 0, 0, 0, 0, 0))
        return 10;
    if (!same(widen(-1L), 0xbf, 0xf0, 0, 0, 0, 0, 0, 0))
        return 11;
    if (!same(widen(0L), 0, 0, 0, 0, 0, 0, 0, 0))
        return 12;
    if (!same((long double) 123456789L, 0x41, 0x9d, 0x6f, 0x34, 0x54, 0, 0, 0))
        return 13;

    /* Passed, returned and chosen between, all of which move eight bytes and
     * change none of them. */
    if (!same(copy(x), 0x40, 0x0a, 0, 0, 0, 0, 0, 0))
        return 14;
    if (!same(pick(1, x, g), 0x40, 0x0a, 0, 0, 0, 0, 0, 0)
        || !same(pick(0, x, g), 0x3f, 0xf8, 0, 0, 0, 0, 0, 0))
        return 15;

    h.tag = 'd';
    h.v = x;
    row[0] = g;
    row[1] = -0.25L;
    if (!same(h.v, 0x40, 0x0a, 0, 0, 0, 0, 0, 0) || h.tag != 'd')
        return 16;
    if (!same(row[0], 0x3f, 0xf8, 0, 0, 0, 0, 0, 0)
        || !same(row[1], 0xbf, 0xd0, 0, 0, 0, 0, 0, 0))
        return 17;

    return 42;
}
