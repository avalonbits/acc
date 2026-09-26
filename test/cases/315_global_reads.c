/* Globals and statics read straight from their addresses, which acc does
 * with ld hl, (nn) and ld a, (nn) where the address was just loaded as a
 * constant: ints, pointers, chars signed and unsigned, shorts and longs
 * beside them, members and elements at constant offsets, externs defined
 * further down, a static local, and a read after a ?: that chose between
 * two addresses, where both ways have to be read -- and a constant added
 * to what was read from an extern, which is not an offset of its address. */
static unsigned long h = 1;

static void mix(long v)
{
    h = (h * 31 + (unsigned long) v) & 0xffffffffUL;
}

struct cfg { char c; int n; unsigned char u; short s; long l; int *p; };

int gi = -123456;
char gc = -7;
unsigned char gu = 250;
signed char gs = -100;
short gsh = -30000;
long gl = -2000000000L;
int *gp;
int arr[5] = { 10, 20, 30, 40, 50 };
char carr[4] = { 'a', -2, 'c', 'd' };
struct cfg cfg = { 'z', 777, 199, -5, 99999L, 0 };
static int si = 4242;

extern int ei;
extern char ec;
extern struct cfg ecfg;

static int counter(void)
{
    static int calls;

    calls++;

    return calls;
}

static int pick(int k)
{
    return *(k ? &gi : &ei) + (k ? *(k > 1 ? &arr[2] : &arr[3]) : cfg.n);
}

int main(void)
{
    int i;

    gp = &arr[1];
    cfg.p = &si;
    mix(gi); mix(gc); mix(gu); mix(gs); mix(gsh); mix(gl); mix(*gp);
    mix(arr[0]); mix(arr[4]); mix(carr[1]); mix(carr[3]);
    mix(cfg.c); mix(cfg.n); mix(cfg.u); mix(cfg.s); mix(cfg.l); mix(*cfg.p);
    mix(si); mix(ei); mix(ec); mix(ecfg.n); mix(ecfg.u); mix(ecfg.c);
    mix(ei + 2); mix(ecfg.n - 3); mix(gi + 2); mix(ec + 1);
    for (i = 0; i < 3; i++)
        mix(counter());
    for (i = 0; i < 3; i++)
        mix(pick(i));
    gi += 5;
    gc = (char) (gc * 3);
    mix(gi); mix(gc);

    /* From gcc on the host. */
    return h == 272971414UL ? 42 : 1;
}

int ei = 31337;
char ec = -1;
struct cfg ecfg = { 'e', -8, 255, 12, -1L, 0 };
