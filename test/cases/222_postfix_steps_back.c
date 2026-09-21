/* x++ answered by stepping back instead of by keeping a copy.
 *
 * The answer x++ wants is the value x had, so the obvious shape reads x
 * twice. Reading a local is six cycles, and a register cannot be duplicated
 * cheaply here -- `ex de, hl` swaps rather than copies. So acc changes x,
 * stores it, and steps back to what it was.
 *
 * That only lands where it started for a value as wide as a register. A
 * narrower one is truncated by the store: an unsigned char at 255 stores 0,
 * and stepping back from 0 gives -1 rather than 255. So the narrow types are
 * here at exactly those edges, where getting the rule wrong shows up, along
 * with the wide ones either side of their own wrap.
 *
 * Pointers too: one whose target is small steps back the same way, and one
 * over a wide struct does not, because the way back would cost more than the
 * read it saved.
 */
static int id(int x) { return x; }

struct wide { char pad[13]; };
static struct wide table[4];
static char bytes[8];

int main(void)
{
    /* Locals, every one of them: a file-scope variable is reached by its
     * address and never goes through the path this is about, so the narrow
     * types have to be on the stack here or the rule that keeps them out of
     * it is not being tested at all. */
    unsigned char uc;
    signed char sc;
    unsigned short us;
    short ss;
    int i;
    unsigned u;
    long l;

    /* The answer is the old value and the variable holds the new one. */
    i = id(10);
    if (i++ != 10) return 1;
    if (i != 11) return 2;
    if (i-- != 11) return 3;
    if (i != 10) return 4;

    /* Off both ends of twenty-four bits, unsigned: a signed int stepping
     * past its largest value is undefined, and agondev's optimiser treats
     * that as a promise while acc wraps, so the two disagree about a value
     * neither is obliged to produce. */
    u = 16777215u;
    if (u++ != 16777215u) return 5;
    if (u != 0u) return 6;
    u = 8388607u;
    if (u++ != 8388607u) return 7;
    if (u != 8388608u) return 8;

    u = 0u;
    if (u-- != 0u) return 9;
    if (u != 16777215u) return 10;

    /* Narrow, at the truncation edge -- where stepping back would lie. */
    uc = 255;
    if (uc++ != 255) return 11;
    if (uc != 0) return 12;
    uc = 0;
    if (uc-- != 0) return 13;
    if (uc != 255) return 14;

    sc = 127;
    if (sc++ != 127) return 15;
    if (sc != -128) return 16;
    sc = -128;
    if (sc-- != -128) return 17;
    if (sc != 127) return 18;

    us = 65535;
    if (us++ != 65535) return 19;
    if (us != 0) return 20;
    ss = 32767;
    if (ss++ != 32767) return 21;
    if (ss != -32768) return 22;

    /* Wider than a register. */
    l = 100;
    if (l++ != 100) return 23;
    if (l != 101) return 24;

    /* Pointers: a small target steps back, a wide one does not. */
    {
        char *p = bytes, *p0 = bytes;
        struct wide *w = table, *w0 = table;

        if (p++ != p0) return 25;
        if (p - p0 != 1) return 26;
        if (p-- != p0 + 1) return 27;
        if (p != p0) return 28;

        /* Against another pointer and not against the array's own name:
         * `w - table` with a thirteen-byte element is miscompiled, and has
         * been since before this was written. Nothing here is about that. */
        if (w++ != w0) return 29;
        if (w - w0 != 1) return 30;
        if (w-- != w0 + 1) return 31;
        if (w != w0) return 32;
    }

    /* The answer thrown away, which is what a for-loop does. */
    {
        int n = 0, k;

        for (k = 0; k < 5; k++)
            n++;
        if (n != 5) return 33;
        if (k != 5) return 34;
    }

    return 42;
}
