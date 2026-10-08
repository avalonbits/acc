/* The answers of &&, || and ?:, which opt-acc's SSA form sets in each
 * path -- 0 and 1, or two constants, or two values -- kept and used as a
 * _Bool, as an int, and as a byte: what the machine-level backend knows
 * of their bits from what every path sets, and nothing more. */
static unsigned char m1 = 3, m2 = 5;

static int scan(const unsigned char *p, int n)
{
    int hits = 0;

    while (n--) {
        _Bool both = p[0] == m1 && p[1] == m2;
        _Bool either = p[0] == m1 || p[1] == m2;
        int pick = p[2] & 4 ? 0x300 : 0x40;
        int other = p[2] & 8 ? p[0] : -p[1];

        if (p[2] & 1) {
            if (both || m1 == 9)
                hits += 100;
        } else if (either) {
            hits += 10;
        }
        hits += both + either + (pick >> 6) + (unsigned char) other;
        p += 3;
    }

    return hits;
}

int main(void)
{
    static const unsigned char data[] = {
        3, 5, 1,   3, 7, 0,   4, 5, 4,   9, 9, 8,   3, 5, 12,
    };
    int got = scan(data, 5);
    int want = (100 + 2 + 1 + 251) + (10 + 1 + 1 + 249) + (10 + 1 + 12 + 251)
               + (0 + 1 + 9) + (10 + 2 + 12 + 3);

    return got == want ? 42 : got & 0x7f;
}
