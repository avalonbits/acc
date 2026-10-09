/* Three shapes the machine IR's splitting allocator gave up on or got
 * wrong, each run here under it (test/modes, "mir split forced"):
 *
 * - scroll: a parameter read back from its slot among a call's arguments,
 *   into a register the call's own value is fixed to. Its interval was cut
 *   at that register's fixed use and not at the call, and the code read a
 *   value no longer there; the verifier refused it.
 * - hex: bytes wanted in A while the others held every register A's class
 *   could give. A held value moved elsewhere frees one.
 * - type_at: a value copied into HL before the sum already in HL was
 *   copied out, two fixed HL intervals at once. The copies swapped. */

typedef unsigned char uint8_t;

struct view { int w, x, y; };
typedef struct { char pad[311]; struct view *v; } screen;

int hidden, shown, tabs;

__attribute__((noinline)) void hide(screen *scr, char ch)
{
    hidden += ch + scr->pad[0];
}

__attribute__((noinline)) int place(screen *scr, const char *pre, int presz)
{
    return scr->v->y < 0 ? presz + pre[0] : 0;
}

__attribute__((noinline)) void tab(screen *scr, int x, int y)
{
    tabs += x * 100 + y + scr->pad[1];
}

__attribute__((noinline)) void show(screen *scr, char ch)
{
    shown += ch + scr->pad[2];
}

__attribute__((noinline)) int scroll(screen *scr, char from_ch, char to_ch,
                                     const char *pre, int presz)
{
    hide(scr, from_ch);
    scr->v->y--;
    const int scrolled = place(scr, pre, presz);
    if (scrolled == 0) {
        tab(scr, scr->v->x, scr->v->y);
        show(scr, to_ch);
    }

    return scrolled;
}

uint8_t hexval[256], shl4[16];

__attribute__((noinline)) int hex(const char *d, int n, int *out)
{
    union {
        int v;
        uint8_t b[sizeof(int)];
    } u;
    uint8_t bad = 0;
    int j = n;

    if (n > 6)
        return 0;
    u.v = 0;
    if (j > 0) {
        uint8_t c = hexval[(uint8_t) d[--j]];
        bad |= c;
        if (j > 0) {
            const uint8_t hi = hexval[(uint8_t) d[--j]];
            bad |= hi;
            c = (uint8_t) (c | shl4[hi & 15]);
        }
        u.b[0] = c;
    }
    if (j > 0) {
        uint8_t c = hexval[(uint8_t) d[--j]];
        bad |= c;
        if (j > 0) {
            const uint8_t hi = hexval[(uint8_t) d[--j]];
            bad |= hi;
            c = (uint8_t) (c | shl4[hi & 15]);
        }
        u.b[1] = c;
    }
    if (j > 0) {
        uint8_t c = hexval[(uint8_t) d[--j]];
        bad |= c;
        if (j > 0) {
            const uint8_t hi = hexval[(uint8_t) d[--j]];
            bad |= hi;
            c = (uint8_t) (c | shl4[hi & 15]);
        }
        u.b[2] = c;
    }
    if ((bad & 0xF0) != 0)
        return 0;
    *out = u.v;

    return 1;
}

struct entry { int a; char type; };
struct entry stack[4] = { { 1, 'a' }, { 2, 'b' }, { 3, 'c' }, { 4, 'd' } };
struct entry *top = stack + 4;
int depth_of = 4, errors;

__attribute__((noinline)) void complain(const char *why)
{
    errors += why[0];
}

__attribute__((noinline)) char type_at(int depth)
{
    if ((unsigned) depth_of <= (unsigned) depth)
        complain("x");

    return (top - 1 - depth)->type;
}

#define CHECK(expr, want) { check++; if ((long) (expr) != (long) (want)) return check; }

int main(void)
{
    struct view view = { 0, 7, 0 };
    screen scr;
    int check = 0, k, value = -1;

    for (k = 0; k != 311; k++)
        scr.pad[k] = (char) k;
    scr.v = &view;
    CHECK(scroll(&scr, 'f', 't', "p", 5), 5 + 'p')
    CHECK(view.y, -1)
    CHECK(hidden, 'f')
    CHECK(tabs + shown, 0)
    view.y = 3;
    CHECK(scroll(&scr, 'g', 'u', "p", 5), 0)
    CHECK(view.y, 2)
    CHECK(hidden, 'f' + 'g')
    CHECK(tabs, 702 + 1)
    CHECK(shown, 'u' + 2)

    for (k = 0; k != 256; k++)
        hexval[k] = 0xff;
    for (k = 0; k != 10; k++)
        hexval['0' + k] = (uint8_t) k;
    for (k = 0; k != 6; k++)
        hexval['a' + k] = hexval['A' + k] = (uint8_t) (10 + k);
    for (k = 0; k != 16; k++)
        shl4[k] = (uint8_t) (k << 4);
    CHECK(hex("1a2B3c", 6, &value), 1)
    CHECK(value, 0x1a2b3cL)
    CHECK(hex("f0e", 3, &value), 1)
    CHECK(value, 0xf0e)
    CHECK(hex("7", 1, &value), 1)
    CHECK(value, 7)
    CHECK(hex("12g4", 4, &value), 0)
    CHECK(value, 7)
    CHECK(hex("1234567", 7, &value), 0)

    CHECK(type_at(0), 'd')
    CHECK(type_at(3), 'a')
    CHECK(errors, 0)
    depth_of = 2;
    CHECK(type_at(3), 'a')
    CHECK(errors, 'x')

    return 42;
}
