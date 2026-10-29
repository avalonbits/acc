#include <stdio.h>
#include <ctype.h>

int main(void)
{
    int c, f;
    static const char *names[] = { "alnum", "alpha", "blank", "cntrl", "digit", "graph",
                                   "lower", "print", "punct", "space", "upper", "xdigit",
                                   "tolower", "toupper" };

    for (f = 0; f < 14; f++) {
        printf("%-7s", names[f]);
        for (c = -1; c < 256; c++) {
            int r;

            switch (f) {
            case 0: r = isalnum(c) != 0; break;
            case 1: r = isalpha(c) != 0; break;
            case 2: r = isblank(c) != 0; break;
            case 3: r = iscntrl(c) != 0; break;
            case 4: r = isdigit(c) != 0; break;
            case 5: r = isgraph(c) != 0; break;
            case 6: r = islower(c) != 0; break;
            case 7: r = isprint(c) != 0; break;
            case 8: r = ispunct(c) != 0; break;
            case 9: r = isspace(c) != 0; break;
            case 10: r = isupper(c) != 0; break;
            case 11: r = isxdigit(c) != 0; break;
            case 12: r = tolower(c) != c; break;
            default: r = toupper(c) != c; break;
            }
            putchar(r ? '1' : '0');
            if ((c & 63) == 62)
                putchar(' ');
        }
        putchar('\n');
    }
    for (c = 0; c < 256; c += 17)
        printf("%d:%d/%d ", c, tolower(c), toupper(c));
    printf("EOF:%d/%d\n", tolower(EOF), toupper(EOF));

    return 0;
}
