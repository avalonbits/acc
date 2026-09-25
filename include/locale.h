/*
 * locale.h -- localization (C99 7.11).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * One locale, "C", which is what every program starts in and what "" -- the
 * native environment -- means on a machine that has no other. setlocale
 * accepts either name for any category and refuses everything else, and
 * localeconv answers with the C locale's values: a point for the decimal
 * point and nothing for the rest.
 */
#ifndef ACC_LOCALE_H
#define ACC_LOCALE_H

/* The names of <stddef.h> this header is given (C99 7.1.3), and no others:
 * a program that includes it may define ptrdiff_t or offsetof itself. */
#ifndef NULL
#define NULL ((void *) 0)
#endif

struct lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char *currency_symbol;
    char  frac_digits;
    char  p_cs_precedes;
    char  n_cs_precedes;
    char  p_sep_by_space;
    char  n_sep_by_space;
    char  p_sign_posn;
    char  n_sign_posn;
    char *int_curr_symbol;
    char  int_frac_digits;
    char  int_p_cs_precedes;
    char  int_n_cs_precedes;
    char  int_p_sep_by_space;
    char  int_n_sep_by_space;
    char  int_p_sign_posn;
    char  int_n_sign_posn;
};

#define LC_ALL      0
#define LC_COLLATE  1
#define LC_CTYPE    2
#define LC_MONETARY 3
#define LC_NUMERIC  4
#define LC_TIME     5

char         *setlocale(int category, const char *locale);
struct lconv *localeconv(void);

#endif
