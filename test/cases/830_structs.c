/* Structs: members of every width at the offsets agondev gives them -- no
 * padding on this target -- reached through a variable, through a pointer,
 * in arrays, nested, assigned whole, and at file scope.
 */
struct point { int x, y; };

struct mixed {
    char c;
    int i;
    long l;
    short s;
    unsigned char u;
    char name[5];
    struct point at;
    struct mixed *next;
};

struct point origin;
struct point corner = { 3, 4 };
struct mixed chain[3];

int sum_points(struct point *p, int n) {
    int total = 0;

    for (int i = 0; i < n; i++)
        total += p[i].x + p->y * 0 + (p + i)->y;

    return total;
}

void move(struct point *p, int dx, int dy) {
    p->x += dx;
    p->y = p->y + dy;
}

int walk(struct mixed *m) {
    int n = 0;

    while (m) {
        n += m->i;
        m = m->next;
    }

    return n;
}

int main(void) {
    int r = 0;
    struct point p;
    struct point q = { 10, 20 };
    struct point ps[4] = { { 1, 2 }, { 3, 4 }, 5, 6 };
    struct mixed m;

    p.x = 1;
    p.y = 2;
    if (p.x + p.y == 3) r++;
    if (q.x == 10 && q.y == 20) r++;

    p = q;                              /* a whole struct */
    if (p.x == 10 && p.y == 20) r++;
    p.x = 7;
    if (q.x == 10) r++;                 /* a copy, not an alias */

    if (ps[2].x == 5 && ps[2].y == 6 && ps[3].x == 0) r++;
    if (sum_points(ps, 3) == 1 + 2 + 3 + 4 + 5 + 6) r++;

    move(&p, 1, -1);
    if (p.x == 8 && p.y == 19) r++;

    m.c = -3;
    m.i = 100000;
    m.l = 70000000;
    m.s = -2;
    m.u = 250;
    m.name[0] = 'a';
    m.name[4] = 'e';
    m.at.x = 11;
    m.at = corner;
    if (m.c == -3 && m.i == 100000 && m.l == 70000000) r++;
    if (m.s == -2 && m.u == 250) r++;
    if (m.name[0] == 'a' && m.name[4] == 'e') r++;
    if (m.at.x == 3 && m.at.y == 4) r++;

    /* Nothing has been stored in these, so they are zero. */
    if (origin.x == 0 && origin.y == 0 && chain[2].i == 0) r++;

    chain[0].i = 1;
    chain[1].i = 2;
    chain[2].i = 4;
    chain[0].next = &chain[1];
    chain[1].next = chain + 2;
    chain[2].next = 0;
    if (walk(chain) == 7) r++;
    if (chain[0].next->next->i == 4) r++;

    {
        struct mixed *pm = &m;
        struct point *pp = &pm->at;
        char *pc = &pm->name[1];

        pp->y++;
        *pc = 'b';
        ++pm->i;
        pm->l -= 1;
        if (m.at.y == 5 && m.name[1] == 'b') r++;
        if (m.i == 100001 && m.l == 69999999) r++;
    }

    corner.x++;
    if (corner.x == 4) r++;

    return r + 25;          /* 17 checks */
}
