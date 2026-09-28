/* Locals declared register, which acc keeps in IY for the whole of their
 * function: read and written there, stepped with inc iy and lea iy, read
 * and written through at (iy+d), and kept across calls and across every
 * one of the backend's own uses of IY -- a long moved three bytes at a
 * time, a short read through a pointer, a mask of three bytes by two, a
 * slot past (ix+d)'s reach -- and across setjmp and longjmp. agondev
 * ignores register, so this is the same program to both.
 */
#include <setjmp.h>

struct node { int value; struct node *next; };

static int count_spaces(register const char *p, const char *e)
{
    int n = 0;

    while (p < e && *p == ' ') {
        p++;
        n++;
    }
    return n + (p == e);
}

static int sum(const int *a, int n)
{
    register const int *p = a;
    int s = 0;

    while (n-- > 0)
        s += *p++;
    return s;
}

static int twice(int x) { return x * 2; }

static int calls(void)
{
    register int k;
    int t = 0;

    for (k = 0; k < 5; k++)
        t += twice(k) + k;              /* 30 */
    return t;
}

static int walk(struct node *head)
{
    register struct node *n;
    int t = 0;

    for (n = head; n; n = n->next) {
        t += n->value;
        n->value = 0;                   /* written through (iy+0) */
    }
    return t;
}

static int steps(void)
{
    static const char s[] = "abcdefghij";
    register const char *p = s;
    int t = 0;

    p += 3;                             /* lea iy, iy+3 */
    t += *p;                            /* 'd' */
    p = p - 2;
    t += p[1];                          /* 'c' */
    (p) = s + 9;                        /* parenthesised, as an object */
    t += *p--;                          /* 'j', and back to 'i' */
    t += *--p;                          /* 'h' */
    t += (int) sizeof p;                /* 3 */
    return t;                           /* 100 + 99 + 106 + 104 + 3 */
}

static long wide(register int k)
{
    long a = 70000L, b;
    short h[2] = { -3, 4 };
    const short *hp = h;

    b = a;                              /* a long copied through IY */
    b = (b << 8) >> 8;                  /* and shifted through it */
    k += *hp + hp[1];                   /* a short read through IY */
    k = (k & 0x00ffff) + (k & 0x00ff);  /* masks that keep a byte in IYL */
    return b + k;
}

static int far(register int k)
{
    int pad[40];                        /* the locals below past (ix+d) */
    int x1 = 1, x2 = 2, x3 = 3, x4 = 4, x5 = 5, x6 = 6, x7 = 7, x8 = 8;
    int x9 = 9, x10 = 10, x11 = 11, x12 = 12, x13 = 13, x14 = 14;
    int x15 = 15, x16 = 16, x17 = 17, x18 = 18, x19 = 19, x20 = 20;
    int x21 = 21, x22 = 22, x23 = 23, x24 = 24, x25 = 25, x26 = 26;
    int x27 = 27, x28 = 28, x29 = 29, x30 = 30, x31 = 31, x32 = 32;
    int x33 = 33, x34 = 34, x35 = 35, x36 = 36;

    pad[0] = k;
    return x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10 + x11 + x12
           + x13 + x14 + x15 + x16 + x17 + x18 + x19 + x20 + x21 + x22
           + x23 + x24 + x25 + x26 + x27 + x28 + x29 + x30 + x31 + x32
           + x33 + x34 + x35 + x36 + pad[0] + k;      /* 666 + 2k */
}

static jmp_buf env;

static void jump(int v) { longjmp(env, v); }

static int jumps(void)
{
    register int kept = 17;
    volatile int got;

    got = setjmp(env);
    if (got == 0)
        jump(5);
    return kept + got;                  /* 22 */
}

int main(void)
{
    static const char s[] = "   x";
    int a[4] = { 1, 2, 3, 4 };
    struct node n2 = { 5, 0 }, n1 = { 7, &n2 };
    int bad = 0;

    bad += count_spaces(s, s + 4) != 3;
    bad += count_spaces(s, s + 2) != 3;
    bad += sum(a, 4) != 10;
    bad += calls() != 30;
    bad += walk(&n1) != 12 || n1.value != 0 || n2.value != 0;
    bad += steps() != 412;
    bad += wide(10) != 70000L + 22;     /* k: 10 + 1 = 11, then 11 + 11 */
    bad += far(3) != 672;
    bad += jumps() != 22;
    return bad ? 100 + bad : 42;
}
