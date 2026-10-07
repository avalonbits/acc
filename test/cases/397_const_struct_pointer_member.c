/* Through a pointer to a const struct, its pointer members are const but
 * what they point at is not: `f->macro->id = 3` writes the macro, as
 * ez80asm's fixup.c does, and `*(o->flag) = v` an int, as its getopt.c
 * does. An array of pointers is the same; a struct inside is const. */
struct macro { int id; };
struct fixup { struct macro *macro; int *flag; struct macro *more[2]; };

static void restore(const struct fixup *f, int id)
{
    if (f->macro)
        f->macro->id = id;
    *(f->flag) = id + 1;
    f->more[1]->id = id + 2;
}

int main(void)
{
    struct macro first = { 0 }, second = { 0 };
    int flag = 0;
    struct fixup fix = { &first, &flag, { 0, &second } };

    restore(&fix, 10);

    return first.id == 10 && flag == 11 && second.id == 12 ? 42 : 1;
}
