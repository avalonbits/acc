/* Struct initialisers -- nested, with braces left out, partial with the rest
 * zeroed, strings in char members -- at file scope and in a function, and
 * the operators on members: compound assignment, steps either side, and
 * `(*p).m`. */
struct inner { char tag; int v[2]; };
struct outer {
    int id;
    struct inner in;
    char name[6];
    long big;
};

struct outer table[3] = {
    { 1, { 'a', { 10, 11 } }, "one", 100000 },
    { 2, 'b', 20, 21, "two" },                  /* braces left out */
    { 3 }                                       /* the rest zero */
};

struct outer single = { 9, { 'z' }, "x" };

int check(struct outer *o, int id, char tag, int v0, int v1, char c0, long big) {
    return o->id == id && o->in.tag == tag && o->in.v[0] == v0
           && o->in.v[1] == v1 && o->name[0] == c0 && o->big == big;
}

int main(void) {
    int r = 0;
    struct outer local[2] = { { 4, { 'c', { 40, 41 } }, "four", -5 }, 5, 'd' };
    struct outer one = { 7, 'e', 70 };
    struct outer *p = table;

    if (check(&table[0], 1, 'a', 10, 11, 'o', 100000)) r++;
    if (check(&table[1], 2, 'b', 20, 21, 't', 0)) r++;
    if (check(&table[2], 3, 0, 0, 0, 0, 0)) r++;
    if (check(&single, 9, 'z', 0, 0, 'x', 0) && single.name[1] == 0) r++;
    if (check(&local[0], 4, 'c', 40, 41, 'f', -5) && local[0].name[3] == 'r') r++;
    if (check(&local[1], 5, 'd', 0, 0, 0, 0)) r++;
    if (check(&one, 7, 'e', 70, 0, 0, 0)) r++;

    (*p).id += 10;
    p->in.v[1] *= 3;
    p[1].big++;
    ++p[1].big;
    if (table[0].id == 11 && table[0].in.v[1] == 33 && table[1].big == 2) r++;
    if (p->in.tag++ == 'a' && table[0].in.tag == 'b') r++;
    if ((p + 2)->id == 3 && (++p)->id == 2) r++;
    if (sizeof(struct outer) == 3 + 7 + 6 + 4 && sizeof table == 60) r++;

    return r + 31;          /* 11 checks */
}
