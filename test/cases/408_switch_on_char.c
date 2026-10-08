/* Switches on a char -- signed and unsigned -- which C makes an int and
 * acc compares as the byte it is, read through a pointer too: cases a
 * byte can have and cases it cannot (200 for a signed char, -1 for an
 * unsigned one, 300 for either), negative ones, a default, and a switch in
 * a loop over a string, nested in another. */
static int classify(signed char c)
{
    switch (c) {
    case 'a': case 'A':
        return 1;
    case -1:
        return 2;
    case -128:
        return 3;
    case 200:                   /* no signed char is this */
        return 4;
    case 300:
        return 5;
    case 0:
        return 6;
    default:
        return 7;
    }
}

static int classify_u(unsigned char c)
{
    switch (c) {
    case 200:
        return 1;
    case -1:                    /* no unsigned char is this */
        return 2;
    case 255:
        return 3;
    case 0:
        return 4;
    default:
        return 5;
    }
}

static int words(const char *p)
{
    int n = 0;

    for (;;) {
        switch (*p++) {
        case 0:
            return n;
        case 'x':
            switch (*p++) {
            case 'y':
                n += 10;
                break;
            case 0:
                return n + 100;
            default:
                n += 1;
            }
            break;
        default:
            n += 2;
        }
    }
}

int main(void)
{
    int ok = 0;

    ok += classify('a') == 1 && classify('A') == 1 && classify(-1) == 2;
    ok += classify(-128) == 3 && classify((signed char) 200) == 7;
    ok += classify((signed char) 300) == 7 && classify(0) == 6;
    ok += classify_u(200) == 1 && classify_u(255) == 3 && classify_u(0) == 4;
    ok += classify_u((unsigned char) -1) == 3 && classify_u(7) == 5;
    ok += words("axyxzq") == 2 + 10 + 1 + 2 && words("bx") == 2 + 100;

    return ok == 6 ? 42 : ok;
}
