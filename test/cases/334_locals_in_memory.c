/* Locals that stay in memory -- arrays, structs, and scalars whose address
 * is taken -- read, written and passed on by opt-acc's own backend: an
 * array's address is the first pass's, patched when the frame is known;
 * a scalar is read and written in its slot, a _Bool as 0 or 1. */
static void bump(int *p) { *p += 5; }
static void set_char(char *p) { *p = -3; }
static void set_flag(_Bool *p, int v) { *p = v; }
static int sum(const char *p, int n)
{
    int total = 0;

    while (n--)
        total += *p++;

    return total;
}

static int array_local(int i, int j, char c)
{
    char buf[8];
    int k;

    for (k = 0; k < 8; k++)
        buf[k] = (char) k;
    buf[i] = c;
    buf[j] = (char) (c + 1);

    return sum(buf + i, 2) + buf[j];
}

static int taken(int start)
{
    int n = start;
    char c = 7;
    _Bool flag = 0;

    bump(&n);
    set_char(&c);
    set_flag(&flag, 256);
    n = n + c;
    flag = flag + 255;          /* 256: true, not its low byte */

    return n + n + flag + c;
}

struct pair { int a, b; };

static void fill(struct pair *p) { p->b = p->a + 1; }

static int struct_local(int v)
{
    struct pair x;

    x.a = v;
    fill(&x);

    return x.a + x.b;
}

static int bump_get(int *p)
{
    *p += 5;

    return *p;
}

/* Compiled in place: the parameter is a slot of the caller's scratch, and
 * its address is taken, so it stays there -- in a slot of this frame's own,
 * moved from where the first pass had it, not over the values in slots
 * live across it. */
__attribute__((always_inline)) static inline int bumped_inline(int v)
{
    return bump_get(&v);
}

static int with_inline(int v)
{
    int one = v + 1, two = v + 2, three = v + 3, four = v + 4;
    int total = bumped_inline(v) - bumped_inline(v + 10);

    return total + one + two + three + four;
}

int main(void)
{
    int score = 0;

    score += array_local(1, 4, 10) == 10 + 2 + 11;
    score += array_local(2, 3, 20) == 20 + 21 + 21;
    score += taken(4) == (4 + 5 - 3) * 2 + 1 - 3;
    score += struct_local(9) == 9 + 10;
    score += with_inline(3) == 8 - 18 + 4 + 5 + 6 + 7;
    return score == 5 ? 42 : score;
}
