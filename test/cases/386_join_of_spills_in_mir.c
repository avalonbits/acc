/* zap's macro_subst, in a function opt-acc's machine-level backend makes:
 * at the loop's head the values it carries meet, most of them in their
 * slots, and each is read from its slot after the copies there -- not
 * reloaded into a register of its own first, which for this join wanted
 * more pairs than there are. The marks substituted, the buffer grown
 * where it is too small, the rest copied after them. */
#include <string.h>

/* The buffer grown from a pool, not the heap: agondev's library, the
 * reference here, has none. */
static char pool[64];
static int pool_used;

static char *grow(char *old, int len, int cap)
{
    char *p = pool + pool_used;

    if (pool_used + cap > (int) sizeof pool)
        return 0;
    pool_used += cap;
    memcpy(p, old, (size_t) len);
    return p;
}

typedef struct { int off; unsigned char k; unsigned char len; } macmark;
typedef struct { const char *body; } macro;

static int margn[4] = { 3, 1, 4, 0 };

static int subst(const macro *m, int lo, int hi, int base, const macmark **mkp,
                 const macmark *mkend, char **bufp, int *capp)
{
    char *out = *bufp;
    int cap = *capp, len = 0, cur = lo, span;
    const macmark *mk = *mkp;

    while (mk < mkend && mk->off < hi) {
        int need = margn[base + mk->k];

        span = mk->off - cur;
        if (len + span + need + 2 > cap) {
            char *grown;

            cap = (len + span + need + 2) * 2;
            grown = grow(out, len, cap);
            if (!grown)
                return -1;
            out = grown;
        }
        memcpy(out + len, m->body + cur, (size_t) span);
        len += span;
        memset(out + len, '0' + mk->k, (size_t) need);
        len += need;
        cur = mk->off + mk->len;
        mk++;
    }
    span = hi - cur;
    if (len + span + 2 > cap)
        return -2;
    memcpy(out + len, m->body + cur, (size_t) span);
    *bufp = out;
    *capp = cap;
    *mkp = mk;
    return len + span;
}

int main(void)
{
    static const macmark marks[] = { { 2, 0, 2 }, { 6, 2, 1 }, { 9, 1, 1 } };
    macro m = { "abXXcdYeZfgh" };
    const macmark *mk = marks;
    static char first[4];
    char *buf = first;
    int cap = 4, n = subst(&m, 0, 12, 0, &mk, marks + 3, &buf, &cap);
    int ok = 0;

    ok += n == 16 && mk == marks + 3 && cap == 32;
    ok += memcmp(buf, "ab000cd2222eZ1gh", 16) == 0;

    return ok * 21;
}
