/* A flexible array member: `char b[];` last in a struct. It takes no room --
 * sizeof the struct is what comes before it -- and stands for whatever was
 * allocated past the struct, which is the caller's business. */
struct header {
    int count;
    char text[];
};

struct pair {
    int a, b;
    int rest[];
};

/* Room for a header and some characters after it, at file scope where the
 * bytes are laid out in order. */
char room[32];
int nums[8];

static int fill(struct header *h, const char *from, int n) {
    int i;

    h->count = n;
    for (i = 0; i < n; i++)
        h->text[i] = from[i];

    return h->count;
}

int main(void) {
    struct header *h = (struct header *) room;
    struct pair *p = (struct pair *) nums;
    struct header plain;

    if (sizeof(struct header) != sizeof(int))
        return 1;
    if (sizeof(struct pair) != 2 * sizeof(int))
        return 2;

    if (fill(h, "abc", 3) != 3)
        return 3;
    if (h->text[0] != 'a' || h->text[2] != 'c')
        return 4;
    h->text[1] = 'z';
    if (h->text[1] != 'z' || h->count != 3)
        return 5;

    /* The member starts where the struct ends. */
    if ((char *) h->text != room + sizeof(struct header))
        return 6;

    p->a = 1;
    p->b = 2;
    p->rest[0] = 3;
    p->rest[1] = 4;
    if (p->a + p->b + p->rest[0] + p->rest[1] != 10)
        return 7;

    /* A struct of that type on its own is legal; the member is simply not
     * there to use. */
    plain.count = 8;
    if (plain.count != 8 || sizeof plain != sizeof(int))
        return 8;

    return 42;
}
