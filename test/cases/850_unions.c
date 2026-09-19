/* Unions: every member at offset zero, as big as the biggest. */
union word {
    long l;
    int i;
    unsigned char b[4];
    struct { char lo, hi; } half;
};

struct tagged {
    char kind;
    union { int n; char *s; } u;
};

union word global_word = { 0x01020304 };

int main(void) {
    int r = 0;
    union word w;
    struct tagged t;

    w.l = 0x11223344;
    if (w.b[0] == 0x44 && w.b[3] == 0x11) r++;
    if (w.half.hi == 0x33) r++;
    w.b[2] = 0;
    if (w.i == 0x3344) r++;
    if (global_word.b[0] == 4 && global_word.b[3] == 1) r++;

    t.kind = 1;
    t.u.n = 12345;
    if (t.u.n == 12345) r++;
    t.u.s = "hi";
    if (t.u.s[1] == 'i') r++;

    return r + 36;          /* 6 checks */
}
