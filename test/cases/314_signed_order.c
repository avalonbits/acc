/* Signed int orderings, which acc compiles as unsigned ones of both sides
 * moved by 0x800000: values at and around the ends of a 24-bit int and
 * around zero, against constants at the same places -- the largest, where
 * x > c cannot become x >= c + 1 -- and against each other, with every
 * operator, the constant on either side, as values and as branches, and in
 * a loop's condition. */
static unsigned long h = 1;

static void mix(long v)
{
    h = (h * 31 + (unsigned long) v) & 0xffffffffUL;
}

#define ONE(x, op, k)   mix(x op k); mix(k op x); if (x op k) mix(3); else mix(5);
#define OPS(x, k)       ONE(x, <, k) ONE(x, <=, k) ONE(x, >, k) ONE(x, >=, k)
#define ALL(x)          OPS(x, -8388607 - 1) OPS(x, -8388607) OPS(x, -1) \
                        OPS(x, 0) OPS(x, 1) OPS(x, 100) OPS(x, 8388606) \
                        OPS(x, 8388607)

static void against_constants(int x)
{
    ALL(x)
}

static void against_each_other(int x, int y)
{
    mix(x < y); mix(x <= y); mix(x > y); mix(x >= y);
    if (x < y) mix(7);
    if (x > y) mix(11);
    if (x <= y) mix(13);
    if (x >= y) mix(17);
}

/* Two members read through pointers, the shape that leaves the right side
 * in BC, from which it is moved without going through DE. */
struct node { int key; struct node *next; };

static int count_below(struct node **head, const struct node *n)
{
    int c = 0;

    while (*head && (*head)->key < n->key) {
        c++;
        head = &(*head)->next;
    }
    if (n->key > (*head ? (*head)->key : 0))
        c += 100;

    return c;
}

int main(void)
{
    static const int v[] = { -8388607 - 1, -8388607, -8388606, -65536, -256,
                             -2, -1, 0, 1, 2, 255, 65535, 8388606, 8388607 };
    int n = (int) (sizeof v / sizeof v[0]), i, j, k;

    for (i = 0; i < n; i++)
        against_constants(v[i]);
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            against_each_other(v[i], v[j]);
    for (k = -5, i = 0; k < 5 && i < 100; k++, i++)
        mix(k);
    for (k = 8388600; k > 8388597; k--)
        mix(k);
    {
        struct node ns[6], *head = &ns[0], probe;

        for (i = 0; i < 6; i++) {
            ns[i].key = v[i * 2 + 1];
            ns[i].next = i < 5 ? &ns[i + 1] : 0;
        }
        for (i = 0; i < n; i++) {
            probe.key = v[i];
            mix(count_below(&head, &probe));
        }
    }

    /* From gcc on the host, whose ints are wider but hold these the same. */
    return h == 732833557UL ? 42 : 1;
}
