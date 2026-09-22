/* The address of something reached through a cast.
 *
 * `&((struct R *) q)->a` is ordinary C: the cast makes a value, the `->`
 * makes that value point at an object, and the object has an address. acc
 * read the operand of `&` as its own small grammar and stopped at the cast,
 * saying a cast has no address -- which is true of the cast and not of what
 * follows it. Three of the gcc torture tests are this, and one of them is
 * the oldest trick in C: offsetof, written as the address of a member of
 * the struct that is not at address zero.
 */
struct inner { int n; char tag; };
struct outer { struct inner first; int rest[4]; char name[8]; };

static char bytes[32];

static int offset_of_rest(void) {
    return (int) (unsigned int) &((struct outer *) 0)->rest[1];
}

int main(void) {
    struct outer o;
    void *p = &o;
    int r = 0;

    o.first.n = 7;
    o.first.tag = 'x';
    o.rest[0] = 100; o.rest[1] = 200; o.rest[2] = 300; o.rest[3] = 400;

    /* A member through a cast, and a member of a member. */
    if (&((struct outer *) p)->first.n == &o.first.n) r++;
    if (((struct inner *) p)->n == 7) r++;

    /* An element through a cast, both with a constant and with an index
     * worked out as it runs. */
    {
        int i = 2;

        if (&((struct outer *) p)->rest[i] == &o.rest[2]) r++;
        if (*&((struct outer *) p)->rest[1] == 200) r++;
    }

    /* A cast to a different pointer type entirely, which is what the bytes
     * of an object are read through. */
    {
        char *b = &((char *) p)[0];

        if (b == (char *) &o) r++;
    }

    /* offsetof, as C was written before <stddef.h> had it: the address of a
     * member of the struct at address zero is how far into it that member
     * is. */
    if (offset_of_rest() == (int) (sizeof(struct inner) + sizeof(int))) r++;

    /* And the same shape on a real object, where the answer has to be the
     * difference between the two addresses. */
    {
        struct outer *q = (struct outer *) bytes;

        if ((char *) &((struct outer *) bytes)->name[3] - (char *) q
            == (int) (sizeof(struct inner) + 4 * sizeof(int) + 3)) r++;
    }

    return r + 35;              /* 7 checks */
}
