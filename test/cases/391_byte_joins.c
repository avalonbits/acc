/* Bytes worked at 24 bits by C's promotions and cut back, which opt-acc's
 * machine-level backend makes byte ops and byte joins: a ?: of two chars
 * returned as a char, a sum and a difference cut to a byte, a join of
 * constants that is a phi's too and read whole as well (acc's own vcmp,
 * whose `op` came out wrong when it was taken for a byte), and a copy of a
 * join read after the join has changed round a loop. */
static char fold(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ? (char) (ch + ('a' - 'A')) : ch;
}

static unsigned char low_sum(unsigned char a, unsigned char b)
{
    return (unsigned char) (a + b - 7);
}

static unsigned char last;

static int pick(int op, int swap)
{
    if (swap)
        op = op == 3 ? 1 : 2;
    last = (unsigned char) op;          /* cut, and read whole below */
    switch (op) {
    case 1: return 10 + last;
    case 2: return 20 + last;
    default: return op + last;
    }
}

static char trail(const char *s)
{
    char prev = 0, now = 0;

    while (*s) {
        prev = now;                     /* the join before it changes */
        now = *s >= '0' && *s <= '9' ? *s : '-';
        s++;
    }

    return (char) (prev + now - '-');
}

/* acc's vcmp: op, swapped to a constant on one path, joined with what it
 * was, and cut to a byte as well as switched on whole. The ?:'s join is
 * read only by the phi after the if -- whose copies come later -- and
 * taken for a byte, it was left unwritten. */
typedef struct { int kind, val; } Value;

static Value stack[4] = { { 1, 2 }, { 3, 4 }, { 5, 6 }, { 7, 8 } };
static Value *vsp = stack + 4;
static unsigned char cmp_op;
static int emitted, forced;

static __attribute__((noinline)) int tok_pair(int op, int with)
{
    return op == with || op == with + 1;
}

static __attribute__((noinline)) void force(Value *v, int r)
{
    forced += v->val * r;
}

static __attribute__((noinline)) void emit(int k)
{
    emitted = emitted * 10 + k;
}

static void cmp(int op)
{
    Value *lhs = vsp - 2, *rhs = vsp - 1;

    if (tok_pair(op, 40)) {
        Value swapped = *lhs;

        *lhs = *rhs;
        *rhs = swapped;
        op = (op == 40) ? 36 : 39;
    }
    force(vsp - 2, 1);
    force(vsp - 1, 2);
    cmp_op = (unsigned char) op;
    switch (op) {
    case 36: emit(1); break;
    case 37: emit(2); break;
    case 38: emit(3); break;
    case 39: emit(4); break;
    default: emit(9);
    }
}

int main(void)
{
    int ok = 0;

    ok += fold('Q') == 'q' && fold('q') == 'q' && fold('@') == '@' && fold('[') == '[';
    ok += low_sum(200, 100) == (unsigned char) 293 && low_sum(3, 2) == (unsigned char) -2;
    ok += pick(3, 1) == 11 && pick(4, 1) == 22 && pick(5, 0) == 10;
    ok += trail("a1b2") == '2' && trail("12") == '6';

    cmp(40);
    cmp(41);
    cmp(37);
    ok += emitted == 142 && cmp_op == 37;

    return ok == 5 ? 42 : ok;
}
