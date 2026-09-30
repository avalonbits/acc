/* A long in opt-acc's own backend where only its low three bytes are kept:
 * read from memory and cut to an int or narrower, and an int stored into a
 * long local, its fourth byte made from its sign -- 0 for one that cannot
 * be negative. zap's parse_operand keeps its numbers so. The functions
 * under test take no long and give none back; the helpers, which do, are
 * the first pass's. Each check on its own. */
static long seen[4];
static long next_value;

static int keep(const long *v, int at)
{
    seen[at] = *v;

    return 0;
}

static void fill(long *v) { *v = next_value; }

/* Ints into long locals, whose addresses are taken: the fourth byte. */
static int widen(int x, unsigned int u)
{
    long a = x, b = u, c = 0, d = -5;

    return keep(&a, 0) + keep(&b, 1) + keep(&c, 2) + keep(&d, 3);
}

/* A long local filled by another, read back as an int and as a char. */
static int narrowed(void)
{
    long v = 0;
    int as_int;

    fill(&v);
    as_int = (int) v;

    return as_int + (signed char) v;
}

/* A long read through a pointer and kept as an int. */
static int through(const long *p)
{
    int low = (int) *p;

    return low - 1;
}

int main(void)
{
    long big = 0x12345678;
    int right = 0;

    widen(-3, 0xfffffe);
    right += seen[0] == -3 && seen[1] == 0xfffffe && seen[2] == 0 && seen[3] == -5;
    next_value = 0x7f001234;
    right += narrowed() == 0x001234 + 0x34;
    next_value = -2;
    right += narrowed() == -2 + -2;
    right += through(&big) == 0x345678 - 1;
    return right == 4 ? 42 : right;
}
