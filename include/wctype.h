/*
 * wctype.h -- wide character classification and mapping (C99 7.25).
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * In the C locale, which is the only one there is: a wide character is in
 * a class when it is an ASCII character in the <ctype.h> class of the same
 * name, and nothing past 127 is in any of them. The mappings change the
 * twenty-six letters and leave everything else alone.
 */
#ifndef ACC_WCTYPE_H
#define ACC_WCTYPE_H

#ifndef ACC_WINT_T
#define ACC_WINT_T
typedef int wint_t;
#endif

typedef int wctrans_t;
typedef int wctype_t;

#ifndef WEOF
#define WEOF ((wint_t) -1)
#endif

int iswalnum(wint_t wc);
int iswalpha(wint_t wc);
int iswblank(wint_t wc);
int iswcntrl(wint_t wc);
int iswdigit(wint_t wc);
int iswgraph(wint_t wc);
int iswlower(wint_t wc);
int iswprint(wint_t wc);
int iswpunct(wint_t wc);
int iswspace(wint_t wc);
int iswupper(wint_t wc);
int iswxdigit(wint_t wc);

int      iswctype(wint_t wc, wctype_t desc);
wctype_t wctype(const char *property);

wint_t    towlower(wint_t wc);
wint_t    towupper(wint_t wc);
wint_t    towctrans(wint_t wc, wctrans_t desc);
wctrans_t wctrans(const char *property);

#endif
