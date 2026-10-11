/* A global not yet defined where it is read -- its address a fixup the
 * end of the file fills in -- read from its address: the load of the
 * address made ld a, (nn), ld hl, (nn), or, where HL was holding the
 * left of an operator or an argument, ld de, (nn) and ld bc, (nn), the
 * fixup moved with it. Ints, signed and unsigned chars, an element and a
 * member, whose address carries what was added to it, and a pointer read
 * then followed. */
extern int counter;
extern int table[4];
extern struct pair { int lo, hi; } span;
extern signed char sc;
extern unsigned char uc;
extern int *where;

static int add3(int a, int b, int c) { return a * 100 + b * 10 + c; }

static int reads(int x)
{
    int r = x + counter;                /* HL holds x */

    r += x * 2 + table[2];
    r += add3(x, span.hi, sc);          /* arguments while HL is busy */
    r += (x + 1) * uc;
    r += x + *where;

    return r;
}

static int picked(int c)
{
    return *(c ? &counter : &table[1]);  /* the second load ends at a join */
}

static int chosen(int c, int x)
{
    return c ? x + counter : x - counter;
}

int counter = 5;
int table[4] = { 1, 2, 30, 4 };
struct pair span = { 7, 8 };
signed char sc = -3;
unsigned char uc = 200;
int *where = &table[3];

int main(void)
{
    if (reads(1) != (1 + 5) + (2 + 30) + (100 + 80 - 3) + (2 * 200) + (1 + 4))
        return 1;
    if (chosen(1, 10) != 15 || chosen(0, 10) != 5)
        return 2;
    if (picked(1) != 5 || picked(0) != 2)
        return 3;

    return 42;
}
