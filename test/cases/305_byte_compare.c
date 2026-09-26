/* A byte compared with a constant, which acc does in A with cp when the
 * constant is in the byte's range: every value of char, signed char and
 * unsigned char, as a local and read through a pointer, against constants
 * inside the range, at its edges and past them, with each operator, the
 * constant on either side, as a value and as a branch. */
static unsigned long h = 1;

static void mix(long v)
{
    h = (h * 31 + (unsigned long) v) & 0xffffffffUL;
}

#define ONE(x, op, k)   mix(x op k); mix(k op x); if (x op k) mix(3); else mix(5);
#define OPS(x, k)       ONE(x, ==, k) ONE(x, !=, k) ONE(x, <, k) \
                        ONE(x, <=, k) ONE(x, >, k) ONE(x, >=, k)
#define ALL(x)          OPS(x, -129) OPS(x, -128) OPS(x, -1) OPS(x, 0) \
                        OPS(x, 127) OPS(x, 128) OPS(x, 255) OPS(x, 256)

static void locals(int i)
{
    char c = i;
    signed char s = i;
    unsigned char u = i;

    ALL(c)
    ALL(s)
    ALL(u)
}

static void pointers(const char *p, const signed char *r, const unsigned char *q)
{
    ALL(*p)
    ALL(*r)
    ALL(*q)
}

int main(void)
{
    int i;

    for (i = 0; i < 256; i++) {
        char c = i;
        signed char s = i;
        unsigned char u = i;

        locals(i);
        pointers(&c, &s, &u);
    }

    /* From gcc on the host, with char signed. */
    return h == 2217997313UL ? 42 : 1;
}
