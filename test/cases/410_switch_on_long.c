/* Switches on a long and an unsigned long, which opt-acc's machine-level
 * backend compares as 24 bits where every case fits in them: values that
 * fit and match, and values whose low 24 bits match a case but whose top
 * byte does not only widen them -- 0x1000001 against case 1, -16777215 --
 * which must go to the default; negative cases; and one switch with a case
 * wider than 24 bits, made the long way. The constants at the edges are
 * written as longs: an unsigned int's 0xffffff, as acc reads it, is
 * widened to a long by its sign. */
static int pick(long v)
{
    switch (v) {
    case 0: return 1;
    case 1: return 2;
    case -1: return 3;
    case 0x7fffff: return 4;
    case -0x800000L: return 5;
    default: return 9;
    }
}

static int pick_u(unsigned long v)
{
    switch (v) {
    case 0: return 1;
    case 5: return 2;
    case 0xffffffUL: return 3;
    default: return 9;
    }
}

static int wide(long v)
{
    switch (v) {
    case 1: return 1;
    case 0x1000001: return 2;
    default: return 9;
    }
}

int main(void)
{
    int ok = 0;

    ok += pick(0) == 1 && pick(1) == 2 && pick(-1) == 3;
    ok += pick(0x7fffffL) == 4 && pick(-0x800000L) == 5 && pick(7) == 9;
    ok += pick(0x1000001L) == 9 && pick(-16777215L) == 9 && pick(0x800000L) == 9;
    ok += pick_u(0) == 1 && pick_u(5) == 2 && pick_u(0xffffffUL) == 3;
    ok += pick_u(0x1000005UL) == 9 && pick_u(0xffffffffUL) == 9;
    ok += wide(1) == 1 && wide(0x1000001L) == 2 && wide(0x2000001L) == 9;

    return ok == 6 ? 42 : ok;
}
