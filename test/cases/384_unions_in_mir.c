/* A union local, written a byte at a time and read as an int, in a
 * function opt-acc's machine-level backend makes: its type code is the one
 * that says only "look at its extension", which is not a long's four
 * bytes, though it is wider than an int's. Its bytes in place, the int
 * read whole. */
static const unsigned char hexval[256] = {
    ['0'] = 0, ['1'] = 1, ['2'] = 2, ['3'] = 3, ['4'] = 4, ['5'] = 5,
    ['6'] = 6, ['7'] = 7, ['8'] = 8, ['9'] = 9, ['a'] = 10, ['b'] = 11,
    ['c'] = 12, ['d'] = 13, ['e'] = 14, ['f'] = 15
};

static int hex(const char *d, int n)
{
    union {
        int v;
        unsigned char b[3];
    } u;
    int j = n;

    u.v = 0;
    if (j > 0) {
        unsigned char c = hexval[(unsigned char) d[--j]];

        if (j > 0)
            c |= (unsigned char) (hexval[(unsigned char) d[--j]] << 4);
        u.b[0] = c;
    }
    if (j > 0) {
        unsigned char c = hexval[(unsigned char) d[--j]];

        if (j > 0)
            c |= (unsigned char) (hexval[(unsigned char) d[--j]] << 4);
        u.b[1] = c;
    }
    if (j > 0) {
        unsigned char c = hexval[(unsigned char) d[--j]];

        if (j > 0)
            c |= (unsigned char) (hexval[(unsigned char) d[--j]] << 4);
        u.b[2] = c;
    }
    return u.v;
}

int main(void)
{
    int ok = 0;

    ok += hex("1", 1) == 1 && hex("ff", 2) == 255;
    ok += hex("abc", 3) == 0xabc && hex("12345", 5) == 0x12345;
    ok += hex("7fffff", 6) == 0x7fffff && hex("", 0) == 0;

    return ok * 14;
}
