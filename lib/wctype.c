/*
 * <wctype.h>: the <ctype.h> classes, for a wide character.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The C locale's classes are ASCII's, and the <ctype.h> functions already
 * answer 0 for anything outside a byte's range -- WEOF included -- so each
 * of these is the narrow one under a wide name. A function of its own each,
 * rather than a macro, so that a program can take its address.
 */
#include <ctype.h>
#include <string.h>
#include <wctype.h>

int iswalnum(wint_t wc)  { return isalnum(wc); }
int iswalpha(wint_t wc)  { return isalpha(wc); }
int iswblank(wint_t wc)  { return isblank(wc); }
int iswcntrl(wint_t wc)  { return iscntrl(wc); }
int iswdigit(wint_t wc)  { return isdigit(wc); }
int iswgraph(wint_t wc)  { return isgraph(wc); }
int iswlower(wint_t wc)  { return islower(wc); }
int iswprint(wint_t wc)  { return isprint(wc); }
int iswpunct(wint_t wc)  { return ispunct(wc); }
int iswspace(wint_t wc)  { return isspace(wc); }
int iswupper(wint_t wc)  { return isupper(wc); }
int iswxdigit(wint_t wc) { return isxdigit(wc); }

wint_t towlower(wint_t wc) { return isupper(wc) ? tolower(wc) : wc; }
wint_t towupper(wint_t wc) { return islower(wc) ? toupper(wc) : wc; }

/* The names wctype knows, in the order of the functions that test them: a
 * class is its place in this list, counted from 1, so that 0 is no class,
 * which is what wctype answers for a name it does not know. */
static const char classes[] =
    "alnum\0alpha\0blank\0cntrl\0digit\0graph\0"
    "lower\0print\0punct\0space\0upper\0xdigit\0";

wctype_t wctype(const char *property)
{
    const char *name = classes;
    int n;

    for (n = 1; *name; n++, name += strlen(name) + 1)
        if (strcmp(name, property) == 0)
            return n;

    return 0;
}

int iswctype(wint_t wc, wctype_t desc)
{
    switch (desc) {
    case 1:  return isalnum(wc);
    case 2:  return isalpha(wc);
    case 3:  return isblank(wc);
    case 4:  return iscntrl(wc);
    case 5:  return isdigit(wc);
    case 6:  return isgraph(wc);
    case 7:  return islower(wc);
    case 8:  return isprint(wc);
    case 9:  return ispunct(wc);
    case 10: return isspace(wc);
    case 11: return isupper(wc);
    case 12: return isxdigit(wc);
    }

    return 0;
}

/* And the two mappings: 1 is to lower case, 2 to upper. */
wctrans_t wctrans(const char *property)
{
    if (strcmp(property, "tolower") == 0)
        return 1;
    if (strcmp(property, "toupper") == 0)
        return 2;

    return 0;
}

wint_t towctrans(wint_t wc, wctrans_t desc)
{
    if (desc == 1)
        return towlower(wc);
    if (desc == 2)
        return towupper(wc);

    return wc;
}
