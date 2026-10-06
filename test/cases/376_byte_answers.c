/* A function answering a char or a _Bool answers it in A, and opt-acc's
 * machine-level backend puts it there without widening it into HL first:
 * a char's low byte, whatever is above it; a _Bool that is 0 or 1 already
 * -- another's answer, a comparison -- as it is; and one that is not, an
 * int of 2, still made 1. */
static int keep(int x) { return x; }
static char low(int x) { return x + 1; }
static signed char neg(int x) { return x - 300; }
static unsigned char masked(const unsigned char *p) { return p[1] & 0x0f; }
static _Bool truth(int x) { return x; }
static _Bool less(int a, int b) { return a < b; }
static _Bool again(int x) { return truth(x); }

int main(void)
{
    static const unsigned char bytes[2] = { 0, 0xab };
    int ok = 0;

    ok += low(0x1234) == 0x35;
    ok += neg(44) == 0;                 /* -256 cut to a byte */
    ok += neg(45) == 1;
    ok += masked(bytes) == 0x0b;
    ok += truth(2) == 1 && keep(truth(2)) == 1;
    ok += less(-3, 2) == 1 && less(2, -3) == 0;
    ok += again(7) == 1 && again(0) == 0;

    return ok * 6;
}
