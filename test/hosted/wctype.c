/* <wctype.h> in the C locale: every class and both mappings, for every
 * character up to past a byte and a few beyond, and WEOF. */
#include <stdio.h>
#include <wctype.h>

static const char *names[] = {
    "alnum", "alpha", "blank", "cntrl", "digit", "graph",
    "lower", "print", "punct", "space", "upper", "xdigit",
};

static int (*const tests[])(wint_t) = {
    iswalnum, iswalpha, iswblank, iswcntrl, iswdigit, iswgraph,
    iswlower, iswprint, iswpunct, iswspace, iswupper, iswxdigit,
};

/* WEOF as -1, since the host's wint_t is unsigned and acc's is not. */
static long show(wint_t wc)
{
    return wc == WEOF ? -1L : (long) wc;
}

static void one(wint_t wc)
{
    char bits[13];
    int i;

    for (i = 0; i < 12; i++) {
        int direct = tests[i](wc) != 0;
        int named = iswctype(wc, wctype(names[i])) != 0;

        bits[i] = direct == named ? (char) ('0' + direct) : '?';
    }
    bits[12] = 0;
    printf("%ld %s %ld %ld %ld %ld\n", show(wc), bits, show(towlower(wc)),
           show(towupper(wc)), show(towctrans(wc, wctrans("tolower"))),
           show(towctrans(wc, wctrans("toupper"))));
}

int main(void)
{
    wint_t wc;

    for (wc = 0; wc < 300; wc++)
        one(wc);
    one(0x3b1);                 /* a Greek alpha */
    one(0x391);
    one(0x7fff);
    one(WEOF);
    printf("unknown %d %d\n", wctype("vowel") == 0, wctrans("rot13") == 0);
    printf("weof %d\n", WEOF == (wint_t) -1);

    return 0;
}
