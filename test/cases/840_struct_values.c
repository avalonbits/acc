/* Structs passed and returned by value, as agondev passes them: the bytes on
 * the stack in whole three-byte slots, and a hidden pointer for the answer.
 */
struct pair { int a; char b; };
struct big { long l; int i; char tag[7]; };

struct pair make(int a, int b) {
    struct pair p;

    p.a = a;
    p.b = b;

    return p;
}

int weigh(struct pair p, int k, struct big g) {
    return p.a * k + p.b + g.i + g.tag[6];
}

struct big grow(struct big g) {
    g.l = g.l * 2;
    g.i++;
    g.tag[6] = 'z';

    return g;
}

struct pair swap(struct pair p) {
    struct pair q = { p.b, p.a };

    return q;
}

union num { long l; char b[4]; };

union num twice(union num u) {
    u.l = u.l * 2;

    return u;
}

void spoil(struct pair p) {
    p.a = 99;           /* the callee's copy */
}

struct pair *pick(struct pair *list, int i) {
    return &list[i];
}

int main(void) {
    int r = 0;
    struct pair p = make(5, 9);
    struct big g;

    if (p.a == 5 && p.b == 9) r++;
    if (make(3, 4).b == 4) r++;
    p = swap(p);
    if (p.a == 9 && p.b == 5) r++;

    g.l = 100000;
    g.i = 1;
    g.tag[6] = 2;
    if (weigh(p, 2, g) == 18 + 5 + 1 + 2) r++;
    g = grow(g);
    if (g.l == 200000 && g.i == 2 && g.tag[6] == 'z') r++;
    if (grow(grow(g)).l == 800000) r++;
    if (weigh(make(1, 2), 3, grow(g)) == 3 + 2 + 3 + 'z') r++;

    {
        struct pair list[3] = { { 1, 2 }, { 3, 4 }, { 5, 6 } };
        union num u;
        int n = 3, m = 4;

        spoil(list[0]);
        if (list[0].a == 1) r++;
        if (weigh(list[1], n * 2 + m, g) == 3 * 10 + 4 + 2 + 'z') r++;
        if (pick(list, 2)->b == 6 && pick(list, 1)[1].a == 5) r++;
        u.l = 0x10203;
        u = twice(u);
        if (u.b[0] == 6 && u.b[2] == 2) r++;
    }

    return r + 31;          /* 11 checks */
}
