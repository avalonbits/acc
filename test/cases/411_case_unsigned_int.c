/* A case's constant an unsigned int -- 0xffffff, and minus 0x800000, which
 * on the Agon is 0x800000 again -- in a switch on a long, an unsigned long
 * and an int: widened by zeros for the longs, as agondev has them, and the
 * same 24 bits as -1 for the int. */

static int on_ulong(unsigned long v)
{
    switch (v) {
    case 0xffffff: return 1;
    case 5: return 2;
    default: return 9;
    }
}

static int on_long(long v)
{
    switch (v) {
    case -0x800000: return 1;
    case 0xffffff: return 2;
    case -1: return 3;
    default: return 9;
    }
}

static int on_int(int v)
{
    switch (v) {
    case 0xffffff: return 1;
    case 0x800000: return 2;
    default: return 9;
    }
}

int main(void)
{
    int ok = 0;

    ok += on_ulong(0xffffffUL) == 1 && on_ulong(0xffffffffUL) == 9;
    ok += on_long(0x800000L) == 1 && on_long(-0x800000L) == 9;
    ok += on_long(0xffffffL) == 2 && on_long(-1L) == 3;
    ok += on_int(-1) == 1 && on_int(0x800000) == 2;

    return ok == 4 ? 42 : ok;
}
