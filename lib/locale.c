/*
 * <locale.h>: the C locale, and nothing else.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <limits.h>
#include <locale.h>
#include <string.h>

/* Only "C" and "", the native locale, which here is the same one; a null
 * name asks which locale is in force, and it is always "C". */
char *setlocale(int category, const char *locale)
{
    if (category < LC_ALL || category > LC_TIME)
        return NULL;
    if (locale && *locale && strcmp(locale, "C") != 0)
        return NULL;

    return "C";
}

/* The values C99 7.11.2.1 gives the C locale. Not const, since C hands the
 * structure out through a pointer to one that is not; a program that writes
 * to it gets what it asked for. */
static struct lconv c_locale = {
    ".", "", "", "", "", "", "", "", "",
    CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX,
    "",
    CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX, CHAR_MAX
};

struct lconv *localeconv(void)
{
    return &c_locale;
}
