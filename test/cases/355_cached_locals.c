/* A local whose address is taken, kept in a register by the leaf backend
 * between whatever may change its memory: every write still goes to
 * memory, and after a call or a store through a pointer it is read from
 * memory again. */
static const unsigned char space[256] = { [' '] = 1, ['\t'] = 1 };

/* Moves *pp past one word, as zap's parsers do with the line pointer. */
static int word(const char **pp)
{
    const char *p = *pp;
    int n = 0;

    while (*p && !space[(unsigned char) *p]) {
        p++;
        n++;
    }
    *pp = p;

    return n;
}

static int words(const char *p)
{
    int total = 0, count = 0;

    for (;;) {
        while (*p && space[(unsigned char) *p])
            p++;                        /* written through: word sees it */
        if (!*p)
            break;
        total += word(&p) * 10;         /* p changed behind its back */
        count++;
    }

    return total + count;
}

/* A step just before a call that reads the local's memory: written
 * through there, not later. */
static int then_call(const char *p)
{
    int n;

    p++;
    n = word(&p);

    return n * 100 + (*p == ' ');
}

/* The value before a call kept, and the one after read again. */
static int before_after(int x)
{
    int before, after;
    int *q = &x;

    before = x;
    *q += 5;                            /* a store through a pointer */
    after = x;
    x++;                                /* x is 6 more than it was */

    return before * 1000 + after * 10 + (*q - before);
}

/* Postfix and prefix values, and a pointer to the local's own memory. */
static int steps(void)
{
    char text[] = "abcdef";
    char *p = text;
    char **pp = &p;
    int sum = 0;

    sum += *p++;                        /* 'a' */
    sum += *++p;                        /* 'c' */
    sum += **pp;                        /* 'c': memory written through */
    *pp += 2;                           /* p moved through the pointer */
    sum += *p;                          /* 'e', read again */

    return sum - 'a' - 'c' - 'c' - 'e';
}

/* Read in a loop, written behind a call in it now and then. */
static void skip2(const char **pp) { *pp += 2; }

static int mixed(const char *p, int n)
{
    int seen = 0;

    while (n-- > 0) {
        seen += *p;
        if (n % 3 == 0)
            skip2(&p);
        else
            p++;
    }

    return seen;
}

int main(void)
{
    static const char text[] = "  ab cde\tf  ";
    static const char digits[] = "0123456789abcdef";
    int right = 0;

    right += words(text) == (2 + 3 + 1) * 10 + 3;
    right += then_call("xabc def") == 301;
    right += before_after(7) == 7000 + 120 + 6;
    right += steps() == 0;
    /* n = 5..1: reads 0, then 1 (n=4), skip at n=3 -> 3, 4 (n=2), 5 (n=1),
     * skip at n=0 */
    right += mixed(digits, 6) == '0' + '1' + '2' + '4' + '5' + '6';
    return right == 5 ? 42 : right;
}
