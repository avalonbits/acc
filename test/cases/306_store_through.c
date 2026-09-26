/* Stores through a pointer, of each width, of constants and of values, and
 * the value each assignment comes to: acc writes a constant straight
 * through the address and a three-byte value from DE or BC, and what is
 * left as the answer has to be what was stored. */
static unsigned long h = 1;

static void mix(long v)
{
    h = (h * 31 + (unsigned long) v) & 0xffffffffUL;
}

struct s { char c; short w; int i; long l; int *p; };

static int twice(int x) { return x + x; }

int main(void)
{
    struct s s, *sp = &s;
    char buf[8];
    int n[4], *np = n, **npp = &np, k = 7, i;
    short w[3];
    unsigned char u[3];

    *buf = 0;
    buf[1] = 'x';
    buf[2] = -1;
    u[0] = 255;
    u[1] = 300;                         /* 44 */
    u[2] = (*buf = 9) + 1;
    mix(buf[0]); mix(buf[1]); mix(buf[2]); mix(u[0]); mix(u[1]); mix(u[2]);

    *w = 0x1234;
    w[1] = -2;
    w[2] = (w[0] = 0x7ff0) + 1;
    mix(w[0]); mix(w[1]); mix(w[2]);

    *n = 0;
    n[1] = -1;
    n[2] = 0x123456;
    n[3] = (n[0] = k + 1) * 2;
    mix(n[0]); mix(n[1]); mix(n[2]); mix(n[3]);
    **npp = twice(k);
    mix(n[0]);
    np[1] = **npp + n[2];
    mix(n[1]);
    mix(*np = k);
    mix(np[2] = *np + 5);
    mix(n[2]);

    sp->c = 'q';
    sp->w = 1000;
    sp->i = -70000;
    sp->l = 100000L;
    sp->p = n;
    sp->p[1] = sp->i;
    mix(sp->c); mix(sp->w); mix(sp->i); mix(sp->l); mix(sp->p - n); mix(n[1]);

    for (i = 0; i < 4; i++)
        n[i] = i * 3 + (np[i] = i);
    mix(n[0] + n[1] + n[2] + n[3]);

    /* From gcc on the host. */
    return h == 780745910UL ? 42 : 1;
}
