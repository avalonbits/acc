/* Values that share a register where one is dead in the other's holes: a
 * loop's phi dead from its last read to the step's copy at the end, two
 * phis that trade values each time round, a value live across a call in
 * one arm and dead in the other, and early exits from inner loops. */

typedef struct { const char *start; char term; } tok_t;

static unsigned char scan(tok_t *t, const char *src)
{
    unsigned char len = 0;

    t->start = src;
    for (;;) {
        char c = *src;

        if (c == 0 || c == ',' || c == ';')
            break;
        src++;
        len++;
        if (c != '\'')
            continue;
        while (*src && *src != '\'') {
            src++;
            len++;
        }
    }
    t->term = *src;

    return len;
}

static int fib(int n)
{
    int a = 0, b = 1;

    while (n-- > 0) {
        int sum = a + b;

        a = b;
        b = sum;
    }

    return a;
}

static int swaps(int n)
{
    int x = 3, y = 7, z = 11;

    while (n-- > 0) {
        int tmp = x;

        x = y;
        y = z;
        z = tmp;
    }

    return x * 100 + y * 10 + z;
}

static int calls;

static int bump(int v)
{
    calls++;

    return v + 1;
}

static int mixed(const int *p, int n)
{
    int total = 0, i;

    for (i = 0; i < n; i++) {
        int v = p[i];

        if (v < 0)
            return -total;
        if (v & 1)
            total += bump(v) + i;
        else
            total += v;
    }

    return total;
}

static int find_pair(const char *s, char a, char b)
{
    int i, j;

    for (i = 0; s[i]; i++) {
        if (s[i] != a)
            continue;
        for (j = i + 1; s[j]; j++)
            if (s[j] == b)
                return i * 16 + j;
    }

    return -1;
}

int main(void)
{
    static const int odd[] = { 1, 2, 3, 4, 5 };
    static const int stop[] = { 2, 4, -1, 9 };
    tok_t tok;
    int ok = 0;

    ok += scan(&tok, "ld a,b") == 4 && tok.term == ',';
    ok += scan(&tok, "'a,b';x") == 7 && tok.term == 0 && scan(&tok, "a;b") == 1 && tok.term == ';';
    ok += scan(&tok, "'unclosed") == 9 && tok.term == 0;
    ok += scan(&tok, "") == 0 && tok.start[0] == 0;
    ok += fib(0) == 0 && fib(1) == 1 && fib(10) == 55 && fib(20) == 6765;
    ok += swaps(0) == 381 && swaps(1) == 813 && swaps(2) == 1137 && swaps(3) == 381;
    ok += mixed(odd, 5) == 24 && calls == 3;
    ok += mixed(stop, 4) == -6 && calls == 3;
    ok += find_pair("abcab", 'b', 'a') == 1 * 16 + 3 && find_pair("xyz", 'x', 'q') == -1;

    return ok == 9 ? 42 : ok;
}
