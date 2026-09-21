/* memcpy, memmove and memset done by the instruction that does them.
 *
 * The eZ80 copies a block with ldir and fills one with ldir reading its own
 * output a byte behind. C says all three a byte at a time, and acc compiled
 * that faithfully into a loop; now a call to one of the three by name goes
 * to a helper written in the instruction instead.
 *
 * ldir reads its count as the full twenty-four bits of BC, so a count of
 * nothing copies sixteen megabytes rather than nothing: the counts here
 * include zero, and check that the byte past the end was left alone as well
 * as that the bytes before it were written.
 *
 * memmove has to work whichever way the blocks overlap. Copying up, where
 * the destination is above the source, has to run backwards or it writes
 * over what it has not read yet; copying down is what ldir already does.
 */
void *memcpy(void *, const void *, unsigned long);
void *memmove(void *, const void *, unsigned long);
void *memset(void *, int, unsigned long);

static unsigned char a[40], b[40];
static int opaque(int x) { return x; }

static void fill(void)
{
    int i;

    for (i = 0; i < 40; i++) {
        a[i] = (unsigned char) (i + 1);
        b[i] = 0;
    }
}

int main(void)
{
    int i, n = opaque(7);

    fill();
    memcpy(b, a, (unsigned long) n);
    for (i = 0; i < n; i++) if (b[i] != (unsigned char) (i + 1)) return 1;
    if (b[n] != 0) return 2;

    /* A count of nothing, which is where the guard is. */
    fill();
    b[0] = 9;
    memcpy(b, a, (unsigned long) opaque(0));
    if (b[0] != 9) return 3;
    memset(b, 'x', (unsigned long) opaque(0));
    if (b[0] != 9) return 4;
    memmove(b, a, (unsigned long) opaque(0));
    if (b[0] != 9) return 5;

    /* A count of one, either side of the guard's edge. */
    fill();
    memcpy(b, a, 1);
    if (b[0] != 1 || b[1] != 0) return 6;
    memset(b, 'z', 1);
    if (b[0] != 'z' || b[1] != 0) return 7;

    fill();
    memset(b, 'q', (unsigned long) n);
    for (i = 0; i < n; i++) if (b[i] != 'q') return 8;
    if (b[n] != 0) return 9;

    /* What they answer is where they put it. */
    fill();
    if (memcpy(b, a, (unsigned long) n) != b) return 10;
    if (memset(b, 0, (unsigned long) n) != b) return 11;
    if (memmove(b, a, (unsigned long) n) != b) return 12;

    /* Overlapping, both ways round. */
    fill();
    memmove(a + 2, a, 8);               /* destination above: backwards */
    if (a[2] != 1 || a[9] != 8) return 13;
    if (a[0] != 1 || a[1] != 2) return 14;

    fill();
    memmove(a, a + 2, 8);               /* destination below: forwards */
    if (a[0] != 3 || a[7] != 10) return 15;

    /* Not overlapping at all, through pointers the compiler cannot see. */
    fill();
    memmove(b + opaque(4), a + opaque(1), (unsigned long) n);
    for (i = 0; i < n; i++) if (b[4 + i] != (unsigned char) (i + 2)) return 16;
    if (b[3] != 0 || b[4 + n] != 0) return 17;

    /* A longer run, past anything a byte of count could hold. */
    fill();
    memset(b, 'k', 40);
    for (i = 0; i < 40; i++) if (b[i] != 'k') return 18;

    return 42;
}
