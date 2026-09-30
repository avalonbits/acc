/* A list walked with its node in IY across a call, by opt-acc's own
 * backend: IY kept in a slot of its own around each call where the node
 * lives across it -- the callee here walks its string in IY too, and a
 * node not put back is its pointer -- `p = p->next` as ld iy, (iy+d), the
 * callee's _Bool tested in A, and the first argument, read through IY,
 * left in HL while the parameter after it goes around it through DE. */
struct node { struct node *next; const char *name; int value; };

/* Its string in IY: what it leaves there is not the caller's. */
static _Bool same(const char *a, register const char *b)
{
    while (*b && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}

static struct node *first(struct node *head, int skip)
{
    while (skip-- && head)
        head = head->next;

    return head;
}

static int find(struct node *head, int skip, const char *name)
{
    struct node *p;

    for (p = first(head, skip); p; p = p->next)
        if (same(p->name, name))
            return p->value;

    return -1;
}

int main(void)
{
    struct node c = { 0, "gamma", 3 }, b = { &c, "beta", 2 }, a = { &b, "alpha", 1 };
    int right = 0;

    right += find(&a, 0, "alpha") == 1;
    right += find(&a, 0, "gamma") == 3;
    right += find(&a, 1, "beta") == 2;
    right += find(&a, 1, "alpha") == -1;
    right += find(&a, 3, "gamma") == -1;
    return right == 5 ? 42 : right;
}
