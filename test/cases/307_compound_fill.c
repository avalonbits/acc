/* A compound literal whose initialiser leaves bytes to be zeroed, or copies
 * a string into a char array, inside an expression that is holding an
 * address in a register: the object assigned to, or the array element. The
 * fill and the copy use HL, DE and BC, and whatever those held has to
 * survive them. */
typedef struct { int a; char s[20]; int b; } T;

static unsigned long h = 1;

static void mix(long v)
{
    h = (h * 31 + (unsigned long) v) & 0xffffffffUL;
}

static void mixt(const T *t)
{
    int i;

    mix(t->a);
    mix(t->b);
    for (i = 0; i < 20; i++)
        mix(t->s[i]);
}

int main(void)
{
    T t, u[3], *tp = &t;
    int k = 5, i;

    for (i = 0; i < 3; i++)
        u[i] = (T) { .a = -1, .b = -1, .s = "junkjunkjunkjunkjun" };

    *tp = (T) { .a = k };
    mixt(&t);
    for (i = 0; i < 3; i++) {
        u[i] = (T) { .b = k + i, .s = "hi" };
        mixt(&u[i]);
    }
    tp = &u[1];
    *tp = (T) { k, { 'x' }, k * 2 };
    mixt(&u[1]);
    mix((u[2] = (T) { .a = 9 }).a);
    mixt(&u[2]);

    /* From gcc on the host. */
    return h == 2639525058UL ? 42 : 1;
}
