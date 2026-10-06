/* A block's static and an inlined body's room, in functions opt-acc's
 * machine-level backend now makes: the static's bytes laid down where the
 * function starts, jumped over, and read where they moved to; the room an
 * always_inline body's locals had in its caller's frame made slots of the
 * frame laid out again -- an array, and a local whose address is taken --
 * each read and written where it is now. */
static void fill(int *p, int n) { int i; for (i = 0; i < n; i++) p[i] = i * 2 + 1; }
static void add_to(int *x, int k) { *x += k; }

static const char *name(int k)
{
    static const char n[] = "statics";

    return n + k;
}

__attribute__((always_inline)) static inline int sum3(int base)
{
    int a[3], s = 0, i;

    fill(a, 3);
    for (i = 0; i < 3; i++)
        s += a[i];
    return s + base;
}

__attribute__((always_inline)) static inline int bumped(int v)
{
    int x = v;

    add_to(&x, 7);
    return x;
}

static int both(int v)
{
    return sum3(v) + bumped(v);
}

int main(void)
{
    int ok = 0;

    ok += name(0)[0] == 's' && name(3)[0] == 't' && name(6)[0] == 's';
    ok += name(7)[0] == 0;
    ok += sum3(1) == 10;
    ok += bumped(5) == 12;
    ok += both(2) == 9 + 2 + 9;
    return ok * 8 + 2;
}
