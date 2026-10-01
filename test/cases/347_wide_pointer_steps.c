/* Pointers stepped by a struct's size in opt-acc's own backend: the int
 * scaled -- doublings and additions, or the helper for a size past what
 * those do cheaply -- the pointer waiting on the stack; and two pointers'
 * difference divided by it. BC kept. Each check on its own. */
struct six { int a, b; };                           /* 6 bytes */
struct thirteen { char name[10]; int value; };      /* 13 bytes */
struct big { char bytes[1000]; };

static struct six sixes[8];
static struct thirteen rows[8];
static struct big bigs[3];

static struct six *six_at(struct six *p, int i) { return p + i; }
static struct thirteen *row_at(int i, struct thirteen *p) { return i + p; }
static struct thirteen *row_back(struct thirteen *p, int i) { return p - i; }
static struct big *big_at(struct big *p, int i) { return p + i; }
static int apart(const struct thirteen *from, const struct thirteen *to) { return (int) (to - from); }

/* The count in BC across the steps. */
static int sum_values(const struct thirteen *p, int n)
{
    int total = 0;

    while (n--) {
        total += p->value;
        p = p + 1;
    }

    return total;
}

/* And by a size the helper scales, the count in BC across it. */
static int big_firsts(const struct big *p, int n)
{
    int total = 0;

    while (n--) {
        total += p->bytes[0];
        p = p + 1;
    }

    return total;
}

int main(void)
{
    int right = 0, k;

    for (k = 0; k < 8; k++)
        rows[k].value = k * 10;
    right += six_at(sixes, 5) == &sixes[5];
    right += row_at(3, rows) == &rows[3] && row_at(-1, &rows[4]) == &rows[3];
    right += row_back(&rows[7], 5) == &rows[2] && row_back(rows + 2, -3) == &rows[5];
    right += big_at(bigs, 2) == &bigs[2];
    right += apart(&rows[1], &rows[6]) == 5 && apart(&rows[6], &rows[1]) == -5;
    right += sum_values(rows + 2, 4) == 20 + 30 + 40 + 50;
    bigs[0].bytes[0] = 1;
    bigs[1].bytes[0] = 2;
    bigs[2].bytes[0] = 4;
    right += big_firsts(bigs, 3) == 7;
    return right == 7 ? 42 : right;
}
