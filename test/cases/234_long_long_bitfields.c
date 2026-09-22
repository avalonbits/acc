/* Bit-fields of 'long long', up to sixty-four bits wide.
 *
 * C99 leaves every bit-field type past int to the implementation, and both
 * compilers here take long long, so acc does too: it turned them down, and
 * twelve of the gcc torture tests wanted them. The bits pack the way every
 * other bit-field does -- byte by byte, a new unit only when the next field
 * would not fit -- which is what agondev does and what this case pins.
 *
 * A field of five, six or seven bytes has no type of its own, so it is
 * reached as eight, which is up to three bytes past the field. Those bytes
 * are read and put back as they were, and the checks below stand a member
 * next to such a field to say that they are.
 *
 * What acc does not do is gcc's arithmetic: gcc works a wide bit-field out
 * in the field's own precision, so `x.b << 32` of a forty-bit field is
 * nothing at all. agondev and acc both give the field its declared type and
 * work in sixty-four bits, which is what the last check says.
 */
struct packed  { unsigned long long a : 8, b : 32; };
struct spread  { long long pad : 12; long long field : 52; };
struct beside  { unsigned long long wide : 40; unsigned char after; };
struct counted { unsigned long long n : 40; };

static struct spread g = { -3, 1234567890123LL };
static struct packed p = { 200, 0xcdef1234 };

int main(void) {
    struct packed a;
    struct spread b;
    struct beside c;
    struct counted d;
    int r = 0;

    /* The unit is the declared type's, so the fields pack into as many
     * bytes as their bits need and no more. */
    if (sizeof(struct packed) == 5 && sizeof(struct spread) == 8) r++;
    if (sizeof(struct beside) == 6) r++;

    /* Written and read back, each keeping its own bits. */
    a.a = 12; a.b = 0xcdef1234;
    if (a.a == 12 && a.b == 0xcdef1234UL) r++;

    /* A signed field, negative and at its largest, sign-extended on the way
     * out of a field narrower than the type that holds it. */
    b.pad = -3; b.field = 0x0004765412345678LL;
    if (b.field == 0x0004765412345678LL && b.pad == -3) r++;
    b.field = -1;
    if (b.field == -1LL && b.pad == -3) r++;

    /* The member after a field that is reached as eight bytes: the three
     * bytes past the field are put back as they were. */
    c.after = 0x5a; c.wide = 0x0100000001ULL;
    if (c.wide == 0x0100000001ULL && c.after == 0x5a) r++;
    c.wide = 0xffffffffffULL;
    if (c.after == 0x5a && c.wide == 0xffffffffffULL) r++;

    /* A global's initial value goes in the same bits. */
    if (g.pad == -3 && g.field == 1234567890123LL) r++;
    if (p.a == 200 && p.b == 0xcdef1234UL) r++;

    /* Counting in a wide field, which reads, adds and writes back. */
    d.n = 0xfffffffeULL;
    d.n++;
    if (d.n == 0xffffffffULL) r++;
    d.n += 2;
    if (d.n == 0x100000001ULL) r++;

    /* The field has the type it was declared with, so a shift is worked out
     * in that type and not in the field's width. */
    if ((d.n << 32) == 0x100000000ULL) r++;

    return r + 30;              /* 12 checks */
}
