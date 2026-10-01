/* Long constants written through pointers by the leaf backend: the low
 * three bytes, then the top one -- through a member of a struct a pointer
 * in IY points at, and through any other pointer -- with the value of the
 * assignment read as an int. */
typedef struct node {
    char *name;
    long addr;
    struct node *next;
} node;

static void reset(node *n, int count)
{
    while (count--) {
        n->addr = 0;
        n->name = 0;
        n++;
    }
}

static int fill(long *p, int count)
{
    int last = 0;

    while (count--)
        last = (int) (*p++ = -5);

    return last;
}

static void big(node *n)
{
    n->addr = 0x12345678;
    n->next->addr = -0x7654321L;
}

static node nodes[3];
static long longs[4];

int main(void)
{
    int right = 0, k;

    for (k = 0; k < 3; k++)
        nodes[k].addr = 77;
    nodes[2].addr = 99;
    reset(nodes, 2);
    right += nodes[0].addr == 0 && nodes[1].addr == 0 && nodes[2].addr == 99;
    right += fill(longs, 3) == -5;
    right += longs[0] == -5 && longs[2] == -5 && longs[3] == 0;
    nodes[0].next = &nodes[1];
    big(&nodes[0]);
    right += nodes[0].addr == 0x12345678 && nodes[1].addr == -0x7654321L;
    return right == 4 ? 42 : right;
}
