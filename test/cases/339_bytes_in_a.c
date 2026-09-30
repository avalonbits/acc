/* Bytes kept in A by opt-acc's own backend: a byte read, and a byte's AND,
 * OR or XOR, left in A for what takes a byte, and widened only for what
 * wants all of an int -- as its type says, a signed char by its sign; a
 * byte put aside with push af and taken back from the high byte of the
 * pair it is popped into. Each check is on its own. */
struct row { unsigned char a0, a1, a2, cc; signed char s; };

/* Terms ORed together, each a byte read through a pointer in IY and
 * ANDed with a byte in a slot: the running OR put aside with push af. */
static int match(const struct row *r, unsigned char m0, unsigned char m1,
                 unsigned char m2)
{
    return (unsigned char) ((r->a0 & m0) | (r->a1 & m1) | (r->a2 & m2)) != 0;
}

/* A signed char in A, put aside, then widened for an int's addition. */
static int signed_sum(const signed char *p)
{
    return p[0] + (p[1] & 0x7f) + p[2];
}

/* Bytes stored from A, and one converted. */
static void copy_masked(unsigned char *to, const unsigned char *from,
                        unsigned char mask, int n)
{
    while (n--)
        *to++ = (unsigned char) (*from++ & mask);
}

/* An unsigned byte in A made a signed char: widened by its sign after. */
static int as_signed(const struct row *r)
{
    return (signed char) (r->a0 | 0xc0) + 100;
}

static int zero_tests(const struct row *r)
{
    int right = 0;

    right += (r->cc & 0x30) == 0;
    right += (unsigned char) (r->a0 ^ r->a1) != 0;
    right += (r->a2 | 0) == 0;

    return right;
}

int main(void)
{
    struct row r = { 0x01, 0x10, 0x00, 0x40, -5 };
    signed char s[3] = { -100, -1, -27 };
    unsigned char in[4] = { 0xf1, 0x22, 0x3c, 0xff }, out[4];
    int right = 0;

    right += match(&r, 0x01, 0x00, 0xff) == 1;
    right += match(&r, 0x02, 0x01, 0xff) == 0;
    right += match(&r, 0x00, 0x10, 0x00) == 1;
    right += signed_sum(s) == -100 + 0x7f - 27;
    copy_masked(out, in, 0x0f, 4);
    right += out[0] == 1 && out[1] == 2 && out[2] == 12 && out[3] == 15;
    right += zero_tests(&r) == 3;
    right += as_signed(&r) == -63 + 100;
    return right == 7 ? 42 : right;
}
