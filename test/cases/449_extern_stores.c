/* An int stored to a global not yet defined where the store is -- its
 * address a fixup the end of the file fills in -- straight to the
 * address, ld (nn), hl, the fixup moved onto it: a number, an int local,
 * an element and a member, whose address carries what was added to it,
 * and the assignment's value used after. A long and a char take the way
 * they took. */
extern int counter;
extern int table[4];
extern struct pair { int lo, hi; } span;
extern long total;
extern char flag;

static void set(int n)
{
    int k = n * 2;

    counter = 5;
    table[2] = k;
    table[3] = 77;
    span.hi = n;
    span.lo = -1;
    total = 100000;
    flag = 9;
}

static int chained(int n)
{
    int k = n;

    return (counter = k) + 1;
}

int counter;
int table[4];
struct pair span;
long total;
char flag;

int main(void)
{
    set(21);
    if (counter != 5 || table[2] != 42 || table[3] != 77 || table[0] != 0)
        return 1;
    if (span.hi != 21 || span.lo != -1 || total != 100000 || flag != 9)
        return 2;
    if (chained(8) != 9 || counter != 8)
        return 3;

    return 42;
}
