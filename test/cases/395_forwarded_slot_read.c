/* A value left on the classic stack for the instruction that reads it, as
 * the slot it was read from rather than loaded: here main's n, the answer
 * of subst read in place, converted to nothing and kept for n == 16. The
 * slot was another value's -- ok's -- once the reading was made, and the
 * hybrid path's n == 16 compared ok's 0 when subst's body was inlined
 * (reduced from test/cases/386). */
#include <string.h>
static char pool[64];
static int pool_used;
static char *grow(char *old, int len, int cap)
{
    char *p = pool + pool_used;
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
    memcpy(out + len, m->body + cur, (size_t) span);
    *bufp = out;
    *capp = cap;
    *mkp = mk;
    return len + span;
}
int main(void)
{
    const macmark marks[] = { { 2, 0, 2 }, { 6, 2, 1 }, { 9, 1, 1 } };
    macro m = { "abXXcdYeZfgh" };
    const macmark *mk = marks;
    char first[4];
    char *buf = first;
    int cap = 4, n = subst(&m, 0, 12, 0, &mk, marks + 3, &buf, &cap);
    int ok = 0;
    ok += n == 16 && mk == marks + 3 && cap == 32;
    ok += memcmp(buf, "ab000cd2222eZ1gh", 16) == 0;
    return ok * 21;
}