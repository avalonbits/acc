/* Shorts as the machine IR holds them: an int in a pair, widened from its
 * two bytes -- by zeros, or by bit 15 -- wherever one is made, and two
 * bytes, no more, wherever one is written. Each in an SSA value, in its
 * slot with its address taken, a global, through a pointer, a parameter
 * and an answer; stepped, converted to, and the answer of an operator
 * narrowed to it -- signed and unsigned, at the values that wrap. The
 * bytes around each one written are checked as they were. */

int opaque[8] = { 32767, -32768, 65535, 40000, -1, 300, 0x123456, 7 };

struct guard { char before; short s; unsigned short u; char after; };
struct guard gg = { 0x5a, 0, 0, 0x5a };
short garr[5] = { 1, -2, 300, -32768, 32767 };
unsigned short ugarr[3] = { 0, 65535, 1 };
short gs = -3;
unsigned short gu = 65535;
char gafter = 0x5a;

static int in(int k) { return opaque[k]; }

static int add_params(short a, unsigned short b) { return a + b; }
static short neg_short(short a) { return -a; }
static unsigned short twice(unsigned short a) { return a * 2; }
static short from_long(long l) { return l; }
static unsigned short ufrom_long(long l) { return (unsigned short) l; }
static int long_to_short(long l)
{
    short s = l;
    unsigned short u = l;

    return s + u;
}

static int long_into_slot(long l)
{
    short m;
    short *pm = &m;
    int r = (m = l);

    return r + *pm;
}

static int long_through(short *p, long l)
{
    *p = l;
    return p[0] + p[1];
}

static int signs(int x)
{
    unsigned short u = x;
    short s = u;
    signed char c = x;
    unsigned short v = c;

    return s + v;
}

static short through(short *p, int add) { *p += add; return *p; }
static unsigned short uthrough(unsigned short *p) { return ++*p; }
static short post_dec(short *p) { return (*p)--; }

static int classify(short s)
{
    switch (s) {
    case -32768: return 1;
    case -1: return 2;
    case 0: return 3;
    case 32767: return 4;
    }
    return 5;
}

static int loop_sum(int n)
{
    short s = 32760;
    unsigned short u = 65530;
    int sum = 0, i;

    for (i = 0; i < n; i++) {
        s++;
        u += 3;
        sum += (s < 0) + (u < 10);
    }
    return sum * 1000 + (s & 0xff) + u;
}

static int slots(int x)
{
    struct guard local;
    short *ps = &local.s;
    unsigned short *pu = &local.u;

    local.before = 0x5a;
    local.after = 0x5a;
    *ps = x;
    *pu = x;
    (*ps)++;
    ++*pu;
    return local.before + local.after + local.s + local.u;
}

static int memory_local(int x)
{
    short s = x;
    unsigned short u = x;
    short *ps = &s;
    unsigned short *pu = &u;
    int a, b;

    a = s++;
    b = --u;
    *ps += 1;
    *pu -= 2;
    return a + b + s + u + (s > 0) + (u > 60000);
}

#define CHECK(expr, want) { check++; if ((long) (expr) != (long) (want)) return check; }

int main(void)
{
    int check = 0;

    CHECK((short) in(0) + 1, 32768)
    CHECK((short) (in(0) + 1), -32768)
    CHECK((unsigned short) in(1), 32768)
    CHECK((short) in(2), -1)
    CHECK((unsigned short) in(4), 65535)
    CHECK((short) in(6), 0x3456)
    CHECK((short) (in(6) + 0x4000), 0x7456)
    CHECK((unsigned short) (in(6) >> 4), 0x2345)
    CHECK(add_params(in(4), in(4)), 65534)
    CHECK(add_params(in(1), in(2)), 32767)
    CHECK(neg_short(in(1)), -32768)
    CHECK(twice(in(3)), 14464)
    CHECK(from_long(0x12348000L + in(7)), -32761)
    CHECK(ufrom_long(-in(7)), 65529)
    CHECK(long_to_short(0x12348000L + in(7)), -32761 + 32775)
    CHECK(long_to_short(-in(7)), -7 + 65529)
    CHECK(long_into_slot(0x12348002L + in(7)), -32759 * 2)
    CHECK(long_into_slot(-in(7)), -14)
    CHECK(signs(in(3)), -25536 + 64)
    CHECK(signs(in(4)), -1 + 65535)
    CHECK(classify(in(1)) * 10000 + classify(in(4)) * 1000 + classify(in(0)) * 100
          + classify(in(2)) * 10 + classify(in(5)), 12425)
    CHECK(loop_sum(in(7) + 3), 7026)
    CHECK(slots(in(0)), 0x5a * 2 - 32768 - 32768 + 65536)
    CHECK(slots(in(4)), 0x5a * 2 + 0 + 0)
    CHECK(memory_local(in(0)), 32767 + 32766 - 32767 + 32764 + 0 + 0)
    CHECK(memory_local(in(4)), -1 + 65534 + 1 + 65532 + 1 + 1)
    CHECK(through(&garr[4], in(7)), -32762)
    CHECK(garr[3] + garr[4] + garr[0], -32768 - 32762 + 1)
    CHECK(uthrough(&ugarr[1]), 0)
    CHECK(ugarr[0] + ugarr[1] + ugarr[2], 1)
    CHECK(post_dec(&garr[3]), -32768)
    CHECK(garr[3], 32767)
    CHECK((gs *= 20000), 5536)
    CHECK((gu += in(7)), 6)
    CHECK(gs + gu + gafter, 5536 + 6 + 0x5a)
    CHECK((gg.s = in(3)) + (gg.u = in(1)), -25536 + 32768)
    CHECK(gg.before + gg.after + gg.s + gg.u, 0x5a * 2 - 25536 + 32768)
    CHECK(++gg.s + gg.u--, -25535 + 32768)
    CHECK(gg.before + gg.after + gg.s + gg.u, 0x5a * 2 - 25535 + 32767)
    CHECK((gs = (short) (in(3) + 1)), -25535)
    CHECK((gs = (short) in(3)) + gs, -51072)
    CHECK(long_through(garr, 0x12348000L + in(7)), -32761 - 2)

    return 42;
}
