/* Locals of sibling scopes that share a frame slot with different types --
 * a char and an unsigned char, as two inlined bodies leave them -- each
 * become values of their own; one whose address is taken stays in memory,
 * even where another of its type is read at that slot later; and a VLA's
 * length, written as an int and read as unsigned by sizeof, is one local
 * however it is read. */
static const unsigned char table[256] = { [' '] = 1, ['\t'] = 1, ['a'] = 2 };

static inline int is_space(char c) { return table[(unsigned char) c] & 1; }
static inline int is_a(unsigned char c) { return (table[c] & 2) != 0; }

static int scan(const char *p, const char *e)
{
    int spaces = 0, as = 0;

    while (p < e) {
        {
            char c = *p;                /* one slot ... */

            spaces += is_space(c) + (c < 0);
        }
        {
            unsigned char u = (unsigned char) *p;   /* ... reused */

            as += is_a(u) + (u > 200);
        }
        p++;
    }

    return spaces * 100 + as;
}

static void put(int *where, int value) { *where = value; }

static int addressed(int n)
{
    int total = 0;

    {
        int x;

        put(&x, n);                     /* x's address taken */
        total += x;
    }
    {
        int y = n * 2;                  /* may share x's slot */

        put(&total, total + y);
        total += y;
    }

    return total;
}

static int aliased(int n)
{
    int z;
    int *alias = &z;                    /* the address first ... */

    z = n;                              /* ... then written by name */
    *alias += 1;

    return z;                           /* and read: n + 1 */
}

static int vla_size(int i, int j)
{
    typedef int row[i + 2];
    int x[4], y[4];

    if (j == 2) {
        x[0] = y[0] = 0;
        return (int) sizeof (row) + x[0];
    }

    return (int) sizeof (row) * 3;
}

int main(void)
{
    static const char text[] = "a b\tc \xc9" "aa";
    int right = 0;

    right += scan(text, text + sizeof text - 1) == 4 * 100 + 4;
    right += addressed(5) == 25 && aliased(5) == 6;
    right += vla_size(20, 3) == 66 * (int) sizeof (int)
             && vla_size(1, 2) == 3 * (int) sizeof (int);
    return right == 3 ? 42 : right;
}
