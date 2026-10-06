/* Locals whose address is taken stay in memory, and opt-acc's machine-level
 * backend reads, writes and steps them in their slots, (ix+d): a char, an
 * int and a pointer changed by the callee they were handed to and read
 * after; ++ and -- of them, a byte in place, a pointer by what it points
 * at; a struct's members and an array's elements at their own
 * displacements; and an array in a frame too big for (ix+d) to reach it,
 * whose address is the first pass's, patched when the function ends. */
struct pt { int x, y; char tag; };
struct big { char pad[13]; };

static void bump_int(int *p) { *p += 10; }
static void bump_char(char *p) { *p += 1; }
static void bump_ptr(struct big **p) { *p += 1; }
static void fill(int *a, int n) { int i; for (i = 0; i < n; i++) a[i] = i * 3; }
static void mark(struct pt *p) { p->x = 7; p->y = 8; p->tag = 'a'; }
static void spread(char *s, int n) { int i; for (i = 0; i < n; i++) s[i] = (char) i; }

static int scalars(void)
{
    int n = 1;
    char c = 126;
    struct big pool[3], *at = pool;

    bump_int(&n);
    bump_char(&c);
    bump_ptr(&at);
    n++;
    c++;                                /* wraps, as a signed char */
    at++;
    return (n == 12) + (c == -128) + (at == pool + 2) + (--n == 11);
}

static int post(void)
{
    int n = 5;
    char c = 0;
    int *p = &n;

    bump_char(&c);
    bump_int(p);
    return (n++ == 15) + (c-- == 1) + (n == 16) + (c == 0);
}

static int members(void)
{
    struct pt p;
    int a[4];

    mark(&p);
    fill(a, 4);
    p.tag++;
    a[2] += p.y;
    return (p.x + p.y == 15) + (p.tag == 'b') + (a[2] == 14) + (a[3] == 9);
}

static int far_array(void)
{
    char s[200];
    int k = 3;

    spread(s, 200);
    bump_int(&k);
    return (s[150] == (char) 150) + (s[199] == (char) 199) + (k == 13);
}

int main(void)
{
    return scalars() * 4 + post() * 3 + members() * 2 + far_array() + 3;
}
