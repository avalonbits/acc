/* A member's bytes reached through a cast of its address and a constant
 * index, which opt-acc's machine-level backend folds into the read or the
 * write -- through a pointer, (iy+d); of a global, (nn); of a local kept
 * in memory, (ix+d) -- and by a cast twice over. */
typedef struct {
    unsigned char tag;
    unsigned int set;
    unsigned char flags;
} entry;

#define BYTE(p, n) (((const unsigned char *) (p))[n])

static entry table[3] = {
    { 1, 0x010203, 0x10 }, { 2, 0x0000f0, 0x20 }, { 3, 0x800000, 0x30 },
};
entry global = { 9, 0x0a0b0c, 0x0d };

static int through_pointer(const entry *e, int n, unsigned int want)
{
    int hits = 0;

    for (; n > 0; n--, e++)
        if ((BYTE(&e->set, 0) & BYTE(&want, 0))
            | (BYTE(&e->set, 1) & BYTE(&want, 1))
            | (BYTE(&e->set, 2) & BYTE(&want, 2)))
            hits += BYTE(e, 0) + BYTE((const char *) &e->flags, 0);

    return hits;
}

static int of_global(void)
{
    ((unsigned char *) &global.set)[1] = 0x55;

    return BYTE(&global.set, 2) + BYTE(&global.set, 1) + BYTE(&global, 4);
}

static int of_local(int k)
{
    entry local;
    entry *p = &local;

    local.set = 0x112233u * (unsigned) k;
    local.flags = 7;
    ((unsigned char *) &p->set)[0] = 0x44;

    return BYTE(&local.set, 0) + BYTE(&local.set, 2) + BYTE(&local, 4);
}

int main(void)
{
    int ok = 0;

    ok += through_pointer(table, 3, 0x000001) == 1 + 0x10;
    ok += through_pointer(table, 3, 0x8000f0) == 2 + 0x20 + 3 + 0x30;
    ok += through_pointer(table, 3, 0x000400) == 0;
    ok += of_global() == 0x0a + 0x55 + 0x0d && global.set == 0x0a550c;
    ok += of_local(1) == 0x44 + 0x11 + 7;

    return ok == 5 ? 42 : ok;
}
