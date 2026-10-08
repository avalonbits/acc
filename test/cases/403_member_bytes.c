/* A member's bytes read and written through a cast of its address and a
 * constant index -- ez80asm's REGSETBYTE, which the leaf backend reads at
 * (iy+d) with the index in the displacement -- forward and back, in a
 * loop over an array of structs. */
typedef struct {
    unsigned char tag;
    unsigned int set;
    unsigned char flags;
} entry;

#define BYTE(p, n) (((const unsigned char *) (p))[n])

static entry table[4] = {
    { 1, 0x010203, 0x10 }, { 2, 0x0000f0, 0x20 },
    { 3, 0x800000, 0x30 }, { 4, 0, 0x40 },
};

static int overlaps(const entry *list, int n, unsigned int want)
{
    int hits = 0;

    for (; n > 0; n--, list++)
        if ((BYTE(&list->set, 0) & BYTE(&want, 0))
            | (BYTE(&list->set, 1) & BYTE(&want, 1))
            | (BYTE(&list->set, 2) & BYTE(&want, 2)))
            hits += BYTE(&list->set, -1) + BYTE(&list->flags, 0);

    return hits;
}

static void mark(entry *list, int n)
{
    for (; n > 0; n--, list++)
        ((unsigned char *) &list->set)[2] |= 0x40;
}

int main(void)
{
    int ok = 0;

    ok += overlaps(table, 4, 0x000001) == 1 + 0x10;
    ok += overlaps(table, 4, 0x8000f0) == 2 + 0x20 + 3 + 0x30;
    ok += overlaps(table, 4, 0x000400) == 0;
    mark(table, 4);
    ok += table[0].set == 0x410203 && table[3].set == 0x400000;
    ok += table[2].set == 0xc00000 && table[1].tag == 2;

    return ok == 5 ? 42 : ok;
}
