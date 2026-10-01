/* What more the leaf backend makes: a short stepped through a pointer, by
 * the first pass's code; a frame past what (ix+d) reaches, its far locals
 * where the arrays are; and two loops whose pointers both live in IY,
 * which they share since neither is live where the other is; and a short
 * made of two bytes, whose operator keeps both, not one. */
struct counter {
    char tag;
    unsigned short count;
    short delta;
};

static int bump_all(struct counter *c, int n)
{
    int total = 0;

    while (n--) {
        c->count++;
        --c->delta;
        total += c->count - c->delta;
        c++;
    }

    return total;
}

static int big_frame(int seed)
{
    char pad[150];
    int a = seed, b = seed * 2, c = seed * 3;

    for (int i = 0; i < 150; i++)
        pad[i] = (char) (i + a);
    a += pad[149] + pad[0];
    b += pad[75];

    return a + b + c;
}

static int two_loops(const char *s, const char *t, int n)
{
    int sum = 0, k = n;

    while (k--)
        sum += *s++;
    k = n;
    while (k--)
        sum -= *t++;

    return sum;
}

static void swap_halves(unsigned short *words, int n)
{
    unsigned char *bytes = (unsigned char *) words;

    for (int i = 0; i < n; i++)
        words[i] = (bytes[i * 2] << 8) | bytes[i * 2 + 1];
}

int main(void)
{
    static unsigned short words[2] = { 0x1234, 0xabcd };
    static struct counter cs[3] = { { 'a', 10, 5 }, { 'b', 65535, 0 }, { 'c', 7, -2 } };
    int right = 0;

    /* 11 - 4, 0 - -1, 8 - -3 */
    right += bump_all(cs, 3) == 7 + 1 + 11 && cs[1].count == 0 && cs[2].delta == -3;
    /* pad[149] = 149 + 1 as a char (-106), pad[0] = 1, pad[75] = 76 */
    right += big_frame(1) == (1 - 106 + 1) + (2 + 76) + 3;
    right += two_loops("abc", "aaa", 3) == 3;
    swap_halves(words, 2);
    right += words[0] == 0x3412 && words[1] == 0xcdab;
    return right == 4 ? 42 : right;
}
