/* Bit-fields, laid out as agondev lays them: read, written, stepped and
 * compound-assigned, signed and unsigned, straddling bytes, a _Bool one,
 * unnamed padding and a zero-width field, and initialised at file scope
 * and in a function. */
struct flags {
    unsigned a : 3;
    unsigned b : 5;
    int c : 4;
    char d;
    unsigned e : 20;
    unsigned f : 9;
    _Bool on : 1;
    int : 2;
    long g : 30;
    unsigned : 0;
    signed char h : 3;
};

struct flags global = { 5, 17, -3, 'x', 1000000, 300, 1, 123456789, -2 };

int main(void) {
    int r = 0;
    struct flags l = { 1, 2, 3, 'y', 4, 5 };
    struct flags *p = &l;

    if (sizeof(struct flags) == 12) r++;
    if (global.a == 5 && global.b == 17 && global.c == -3 && global.d == 'x') r++;
    if (global.e == 1000000 && global.f == 300 && global.on == 1) r++;
    if (global.g == 123456789 && global.h == -2) r++;
    if (l.a == 1 && l.b == 2 && l.c == 3 && l.e == 4 && l.f == 5
        && l.on == 0 && l.g == 0 && l.h == 0) r++;

    p->a = 9;                   /* 1, in three bits */
    p->c = 7;
    p->c++;                     /* -8 */
    p->e += 1048574;            /* past twenty bits: wraps to 2 */
    l.on = 5;                   /* a _Bool: 1 */
    l.g = -1;
    l.h = 3;
    if (l.a == 1 && l.b == 2 && l.c == -8 && l.d == 'y') r++;
    if (l.e == 2 && l.f == 5 && l.on == 1 && l.g == -1 && l.h == 3) r++;
    if ((l.f = 513) == 1 && (l.c = 9) == -7) r++;   /* the value it holds */
    if (l.b - 3 < 0) r++;       /* an int, not unsigned, once read */

    return r + 33;          /* 9 checks */
}
