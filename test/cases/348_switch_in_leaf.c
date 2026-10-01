/* A switch in opt-acc's own backend: its value from its slot into HL, and
 * each case ld de, value; or a; sbc hl, de; add hl, de; jp z -- HL as it
 * was for the next. Each check on its own. */
static int classify(int x)
{
    switch (x) {
    case -100000: return 1;
    case -1:      return 2;
    case 0:       return 3;
    case 7:
    case 8:       return 4;             /* one target for two */
    case 0x7fffff: return 5;
    default:      return 6;
    }
}

static int fall(int x)
{
    int r = 0;

    switch (x) {
    case 1: r += 1;
    case 2: r += 10;
        break;
    case 3: r += 100;
    default: r += 1000;
    }

    return r;
}

static int on_char(const char *s)
{
    int vowels = 0;

    for (; *s; s++)
        switch (*s) {
        case 'a': case 'e': case 'i': case 'o': case 'u':
            vowels++;
            break;
        default:
            break;
        }

    return vowels;
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
        return 20 + b;
    }

    return -1;
}

int main(void)
{
    int right = 0;

    right += classify(-100000) == 1 && classify(-1) == 2 && classify(0) == 3;
    right += classify(7) == 4 && classify(8) == 4 && classify(0x7fffff) == 5
             && classify(9) == 6 && classify(-99999) == 6;
    right += fall(1) == 11 && fall(2) == 10 && fall(3) == 1100 && fall(4) == 1000;
    right += on_char("education is key") == 7;
    right += nested(1, 1) == 11 && nested(1, 2) == 12 && nested(1, 3) == 10
             && nested(2, 5) == 25 && nested(3, 0) == -1;
    return right == 5 ? 42 : right;
}
