/* memcpy, memmove, memset and memchr in functions opt-acc's machine-level
 * backend makes: each made in place, a call of the runtime's routine with
 * its operands in registers -- how many in BC, the source in HL and the
 * destination in DE, or the pointer in HL and the byte in A -- and what
 * is live across it kept, as round any call. Answers used, overlapping
 * moves both ways, a byte not found, a count of 0. */
#include <string.h>

struct rec {
    char name[8];
    int n;
};

static int copied(char *d, const char *s, int n, int k)
{
    char *r = memcpy(d, s, n);

    return (r == d) + d[0] + d[n - 1] + k;
}

static void cleared(struct rec *p, int k)
{
    memset(p->name, k, sizeof p->name);
    p->n = k + 1;
}

static int found(const char *p, int n, int c)
{
    const char *at = memchr(p, c, n);

    return at ? (int) (at - p) : -1;
}

static void opened(char *p, int n)
{
    memmove(p + 1, p, n);
    p[0] = '_';
}

static void closed(char *p, int n)
{
    memmove(p, p + 1, n);
}

int main(void)
{
    char buf[16], tmp[16];
    struct rec r;
    int ok = 0;

    ok += copied(buf, "abcdef", 6, 100) == 1 + 'a' + 'f' + 100 && buf[3] == 'd';
    cleared(&r, 'z');
    ok += r.name[0] == 'z' && r.name[7] == 'z' && r.n == 'z' + 1;
    ok += found("hello", 5, 'l') == 2 && found("hello", 5, 'q') == -1 && found("hello", 0, 'h') == -1;
    memcpy(tmp, "abcd", 5);
    opened(tmp, 4);
    ok += tmp[0] == '_' && tmp[1] == 'a' && tmp[4] == 'd';
    closed(tmp, 4);
    ok += tmp[0] == 'a' && tmp[3] == 'd' && tmp[4] == 'd';
    memset(tmp, 'q', 0);
    ok += tmp[0] == 'a';

    return ok * 7;
}
