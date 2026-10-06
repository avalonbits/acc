/* A switch made by opt-acc's machine-level backend: its value read from
 * its slot once and each case a comparison and a branch -- an int's by
 * ld de, n / or a / sbc hl, de / add hl, de, HL kept for the next, 0 by
 * the test against 0, a char's by cp n, keeping A, and no test at all
 * for a value the char cannot have. Values in all three bytes, negative
 * ones, cases falling into the next, a default, a switch in a loop and
 * one inside another's case. */
static int pick(int x)
{
    switch (x) {
    case 0: return 1;
    case 1: return 2;
    case -1: return 3;
    case 0x123456: return 4;
    case 0x8000: return 5;
    case -300: return 6;
    }
    return 7;
}

static int chars(signed char c)
{
    switch (c) {
    case 'a': return 1;
    case -5: return 2;
    case 200: return 3;                 /* never, for a signed char */
    case 0: return 4;
    default: return 5;
    }
}

static int bytes(unsigned char c)
{
    switch (c) {
    case 200: return 1;
    case -1: return 2;                  /* never, for an unsigned char */
    case 255: return 3;
    }
    return 4;
}

static int falls(int x)
{
    int s = 0;

    switch (x & 7) {
    case 1:
        s += 10;
    case 2:
        s += 20;
        break;
    case 3:
        s = 1;
        break;
    default:
        s = -1;
    }
    return s;
}

static int nested(int a, int b)
{
    switch (a) {
    case 1:
        switch (b) {
        case 1: return 11;
        case 2: return 12;
        }
        return 10;
    case 2:
        return 20;
    }
    return 0;
}

static int loop(const char *s)
{
    int n = 0;

    for (; *s; s++)
        switch (*s) {
        case ' ': n += 1; break;
        case 'x': n += 10; break;
        case '\n': n += 100; break;
        }
    return n;
}

int main(void)
{
    int ok = 0;

    ok += pick(0) == 1 && pick(1) == 2 && pick(-1) == 3 && pick(0x123456) == 4;
    ok += pick(0x8000) == 5 && pick(-300) == 6 && pick(2) == 7 && pick(0x123457) == 7;
    ok += chars('a') == 1 && chars(-5) == 2 && chars(-56) == 5 && chars(0) == 4 && chars(1) == 5;
    ok += bytes(200) == 1 && bytes(255) == 3 && bytes(1) == 4;
    ok += falls(1) == 30 && falls(2) == 20 && falls(3) == 1 && falls(0) == -1 && falls(9) == 30;
    ok += nested(1, 1) == 11 && nested(1, 2) == 12 && nested(1, 3) == 10 && nested(2, 1) == 20;
    ok += nested(3, 1) == 0 && loop("x x\n ") == 122;

    return ok * 6;
}
