/* <locale.h>: the C locale, which is the one every program starts in and
 * what the empty name gives when the host's environment says C too. */
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>

static void show(const char *what, const char *s)
{
    printf("%s: %s\n", what, s ? s : "(null)");
}

static void chars(const char *what, int c)
{
    printf("%s: %s\n", what, c == CHAR_MAX ? "CHAR_MAX" : "other");
}

int main(void)
{
    struct lconv *lc;
    int cats[] = { LC_ALL, LC_COLLATE, LC_CTYPE, LC_MONETARY, LC_NUMERIC,
                   LC_TIME };
    int i;

    for (i = 0; i < 6; i++) {
        show("query", setlocale(cats[i], NULL));
        show("C", setlocale(cats[i], "C"));
        show("native", setlocale(cats[i], ""));
        show("klingon", setlocale(cats[i], "tlh_KL"));
    }
    printf("distinct %d\n", LC_ALL != LC_CTYPE && LC_TIME != LC_NUMERIC);

    lc = localeconv();
    show("decimal_point", lc->decimal_point);
    show("thousands_sep", lc->thousands_sep);
    printf("grouping %d\n", (int) strlen(lc->grouping));
    show("mon_decimal_point", lc->mon_decimal_point);
    show("mon_thousands_sep", lc->mon_thousands_sep);
    printf("mon_grouping %d\n", (int) strlen(lc->mon_grouping));
    show("positive_sign", lc->positive_sign);
    show("negative_sign", lc->negative_sign);
    show("currency_symbol", lc->currency_symbol);
    show("int_curr_symbol", lc->int_curr_symbol);
    chars("frac_digits", lc->frac_digits);
    chars("p_cs_precedes", lc->p_cs_precedes);
    chars("n_cs_precedes", lc->n_cs_precedes);
    chars("p_sep_by_space", lc->p_sep_by_space);
    chars("n_sep_by_space", lc->n_sep_by_space);
    chars("p_sign_posn", lc->p_sign_posn);
    chars("n_sign_posn", lc->n_sign_posn);
    chars("int_frac_digits", lc->int_frac_digits);
    chars("int_p_cs_precedes", lc->int_p_cs_precedes);
    chars("int_n_cs_precedes", lc->int_n_cs_precedes);
    chars("int_p_sep_by_space", lc->int_p_sep_by_space);
    chars("int_n_sep_by_space", lc->int_n_sep_by_space);
    chars("int_p_sign_posn", lc->int_p_sign_posn);
    chars("int_n_sign_posn", lc->int_n_sign_posn);

    return 0;
}
