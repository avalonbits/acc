/* A long stored through a pointer to something narrower -- a char, an int,
 * a _Bool, as `*p = l` or a global's `g = l` -- writes that, and no more:
 * the machine IR wrote all four bytes of the long, over what came after.
 * A _Bool is made 1 by a long whose low bytes are all 0. */

long opaque[2] = { 0x12345678L, 0x1000000L };

struct after_char { char c; char rest[3]; };
struct after_int { int i; char rest[3]; };
struct after_bool { _Bool b; char rest[3]; };

struct after_char gc = { 0, { 0x5a, 0x5a, 0x5a } };
struct after_int gi = { 0, { 0x5a, 0x5a, 0x5a } };
struct after_bool gb = { 0, { 0x5a, 0x5a, 0x5a } };

static void to_char(char *p, long l) { *p = l; }
static void to_int(int *p, long l) { *p = l; }
static void to_bool(_Bool *p, long l) { *p = l; }
static void globals(long l) { gc.c = l; gi.i = l; gb.b = l; }

static int rest(const char *r) { return r[0] == 0x5a && r[1] == 0x5a && r[2] == 0x5a; }

static int bool_local(long l)
{
    _Bool b;
    _Bool *pb = &b;

    b = l;
    return *pb;
}

int main(void)
{
    struct after_char c = { 0, { 0x5a, 0x5a, 0x5a } };
    struct after_int i = { 0, { 0x5a, 0x5a, 0x5a } };
    struct after_bool b = { 0, { 0x5a, 0x5a, 0x5a } };

    to_char(&c.c, opaque[0]);
    to_int(&i.i, opaque[0]);
    to_bool(&b.b, opaque[1]);
    if (c.c != 0x78 || !rest(c.rest))
        return 1;
    if (i.i != 0x345678 || !rest(i.rest))
        return 2;
    if (b.b != 1 || !rest(b.rest))
        return 3;
    globals(opaque[1]);
    if (gc.c != 0 || !rest(gc.rest) || gi.i != 0 || !rest(gi.rest)
        || gb.b != 1 || !rest(gb.rest))
        return 4;
    if (bool_local(opaque[1]) != 1)
        return 5;

    return 42;
}
