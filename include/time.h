/* SPDX-License-Identifier: LGPL-2.1-or-later */
#pragma once
#ifndef ACC_TIME_H
#define ACC_TIME_H

/* The names of <stddef.h> this header is given (C99 7.1.3), and no others:
 * a program that includes it may define ptrdiff_t or offsetof itself. */
#ifndef ACC_SIZE_T
#define ACC_SIZE_T
typedef unsigned int size_t;
#endif
#ifndef NULL
#define NULL ((void *) 0)
#endif

/* MOS counts hundredths of a second, so that is what a clock tick is here --
 * the same figure agondev uses, and the reason a program can divide by it
 * without reaching for floating point. */
#define CLOCKS_PER_SEC 100UL

typedef unsigned long clock_t;

/* Seconds since the start of 1970, as everywhere else, and in 64 bits so
 * that 2038 is not the end of it. The Agon's clock keeps no time zone, so
 * the time it keeps is taken as UTC and local time is the same. */
typedef long long time_t;

struct tm {
    int tm_sec;                 /* 0 to 60 */
    int tm_min;
    int tm_hour;
    int tm_mday;                /* 1 to 31 */
    int tm_mon;                 /* 0 to 11 */
    int tm_year;                /* since 1900 */
    int tm_wday;                /* 0 to 6, from Sunday */
    int tm_yday;                /* 0 to 365 */
    int tm_isdst;
};

clock_t    clock(void);
double     difftime(time_t t1, time_t t0);
time_t     mktime(struct tm *t);
time_t     time(time_t *t);

char      *asctime(const struct tm *t);
char      *ctime(const time_t *t);
struct tm *gmtime(const time_t *t);
struct tm *localtime(const time_t *t);
size_t     strftime(char *s, size_t max, const char *fmt, const struct tm *t);

#endif
