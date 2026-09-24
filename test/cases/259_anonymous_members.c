/* C11's anonymous structs and unions: a member that is a struct or union
 * with no tag and no name, whose own members are named as though they
 * were the enclosing record's. agondev's <agon/mos.h> keeps the pixel read
 * off the screen this way, as three bytes and as one number at once, and
 * acc refused the header. */
#define offsetof(t, m) __builtin_offsetof(t, m)

typedef struct {
    char kind;
    union {
        long whole;
        struct {
            unsigned char lo, mid, hi, top;
        };
    };
    struct {
        short x, y;
        union {
            int w;
            char c;
        };
    };
    int after;
} Rec;

Rec g = { 1, { 0x04030201L }, { 5, 6, { 7 } }, 8 };

int main(void) {
    Rec r = { .kind = 2, .whole = 0x44332211L, .y = 9, .c = 10, .after = 11 };
    Rec *p = &r;
    int n = 0;

    if (g.lo == 1 && g.mid == 2 && g.hi == 3 && g.top == 4) n++;
    if (g.x == 5 && g.y == 6 && g.w == 7 && g.after == 8) n++;
    if (r.kind == 2 && r.lo == 0x11 && p->top == 0x44 && r.y == 9 && p->c == 10
        && r.after == 11) n++;
    p->mid = 0x77;
    if (r.whole == 0x44337711L) n++;
    if (offsetof(Rec, top) == offsetof(Rec, whole) + 3 && offsetof(Rec, whole) == 1
        && offsetof(Rec, c) == offsetof(Rec, w)) n++;
    if (sizeof (Rec) == 1 + 4 + 2 + 2 + 3 + 3) n++;

    return n + 36;              /* 6 checks */
}
