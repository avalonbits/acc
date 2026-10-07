/* Structs answered and passed by value, in functions opt-acc's machine-
 * level backend makes: each call answering one given room of its own in
 * the frame, whose address goes ahead of the arguments; each argument of
 * one pushed as three-byte words from where it is -- a local, a global, a
 * struct read through a pointer, another call's answer -- or, past three
 * words, as gen_call copies it; and a return of one copied to where the
 * caller asked. Three, six and twenty-one bytes. */
typedef struct { char ch; char vkey; char mods; } key;
typedef struct { int line, x; } pos;
typedef struct { int a[7]; } big;

static key g_key = { 'g', 7, 1 };
static int at = 123;

static __attribute__((noinline)) key make_key(int k)
{
    key kp;

    kp.ch = (char) ('a' + k);
    kp.vkey = (char) (k * 2);
    kp.mods = (char) (k & 1);

    return kp;
}

static __attribute__((noinline)) key key_at(const key *p)
{
    return *p;
}

static __attribute__((noinline)) key global_key(void)
{
    return g_key;
}

static __attribute__((noinline)) pos tell(void)
{
    pos p;

    p.line = at / 10;
    p.x = at % 10;

    return p;
}

static __attribute__((noinline)) big fill(int base)
{
    big b;
    int i;

    for (i = 0; i < 7; i++)
        b.a[i] = base + i;

    return b;
}

static __attribute__((noinline)) int sum_key(int a, key k, int b)
{
    return a + k.ch * 2 + k.vkey * 3 + k.mods * 5 + b;
}

static __attribute__((noinline)) int sum_two(key k, pos p)
{
    return k.ch + p.line * 1000 + p.x;
}

static __attribute__((noinline)) int sum_big(big b, int k)
{
    return b.a[0] + b.a[6] * k;
}

static int answers(void)
{
    key k = make_key(3);
    key copy = key_at(&k);
    pos p = tell();

    global_key();                       /* answered, and let go */
    return (k.ch == 'd') + (copy.vkey == 6) + (global_key().ch == 'g')
           + (p.line == 12) + (tell().x == 3);
}

static int arguments(void)
{
    key k = make_key(2);
    key *pk = &g_key;
    pos p = tell();
    big b = fill(10);

    return (sum_key(1, k, 2) == 3 + 'c' * 2 + 4 * 3)
           + (sum_key(0, g_key, 0) == 'g' * 2 + 21 + 5)
           + (sum_key(0, *pk, 1) == 'g' * 2 + 21 + 5 + 1)
           + (sum_key(0, make_key(1), 0) == 'b' * 2 + 6 + 5)
           + (sum_two(k, p) == 'c' + 12003)
           + (sum_big(b, 2) == 10 + 32)
           + (sum_big(fill(1), 1) == 1 + 7);
}

int main(void)
{
    return answers() == 5 && arguments() == 7 ? 42 : 1;
}
