/*
 * strftime, wcsftime, asctime and ctime: a time written out.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Every conversion C99 7.23.3.5 has, in the C locale, with the E and O
 * modifiers accepted and making no difference, as they make none there.
 * The time zone is UTC, since the Agon keeps none: see <time.h>.
 *
 * One writer for the narrow and the wide functions, which hands each
 * character to a function; asctime is the format C99 gives for it.
 */
#include <stddef.h>
#include <time.h>
#include <wchar.h>

static const char *const days[7] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
static const char *const months[12] = {
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December"
};

typedef struct {
    void  *to;
    size_t max, n;
    int    wide;
} Out;

static void put(Out *o, int c)
{
    if (o->n < o->max) {
        if (o->wide)
            ((wchar_t *) o->to)[o->n] = (wchar_t) c;
        else
            ((char *) o->to)[o->n] = (char) c;
    }
    o->n++;
}

static void puts_n(Out *o, const char *s, int n)
{
    while (*s && n-- > 0)
        put(o, *s++);
}

/* v in at least `width` digits, with `fill` in front. */
static void number(Out *o, long v, int width, int fill)
{
    char d[12];
    int n = 0;
    unsigned long u = v < 0 ? 0ul - (unsigned long) v : (unsigned long) v;

    do
        d[n++] = (char) ('0' + u % 10);
    while (u /= 10);
    if (v < 0) {
        width--;
        put(o, '-');
    }
    while (width-- > n)
        put(o, fill);
    while (n)
        put(o, d[--n]);
}

/* The weekday of 1 January of year y, and so how many ISO weeks it has:
 * 53 when it starts on a Thursday, or on a Wednesday in a leap year. */
static int iso_weeks(long y)
{
    long p = (y + y / 4 - y / 100 + y / 400) % 7;
    long q = ((y - 1) + (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400) % 7;

    return 52 + (p == 4 || q == 3);
}

/* The ISO 8601 week and the year it belongs to, which near New Year may be
 * the year before or after. */
static int iso_week(const struct tm *t, long *year)
{
    int wd = (t->tm_wday + 6) % 7 + 1;          /* Monday is 1 */
    int w = (t->tm_yday + 1 - wd + 10) / 7;

    *year = t->tm_year + 1900L;
    if (w < 1) {
        --*year;
        w = iso_weeks(*year);
    } else if (w > iso_weeks(*year)) {
        ++*year;
        w = 1;
    }

    return w;
}

static void format(Out *o, const char *fmt, int fs, const struct tm *t)
{
#define F(q) (fs == 1 ? *(const unsigned char *) (q) : *(const unsigned short *) (q))
    long year = t->tm_year + 1900L, iso_year;
    int hour12 = t->tm_hour % 12 ? t->tm_hour % 12 : 12;

    for (; F(fmt); fmt += fs) {
        int c = F(fmt);

        if (c != '%') {
            put(o, c);
            continue;
        }
        fmt += fs;
        if (F(fmt) == 'E' || F(fmt) == 'O')
            fmt += fs;
        switch (F(fmt)) {
        case 'a': puts_n(o, days[t->tm_wday % 7], 3); break;
        case 'A': puts_n(o, days[t->tm_wday % 7], 99); break;
        case 'b':
        case 'h': puts_n(o, months[t->tm_mon % 12], 3); break;
        case 'B': puts_n(o, months[t->tm_mon % 12], 99); break;
        case 'c': format(o, "%a %b %e %H:%M:%S %Y", 1, t); break;
        case 'C': number(o, year / 100 - (year % 100 < 0), 2, '0'); break;
        case 'd': number(o, t->tm_mday, 2, '0'); break;
        case 'D': format(o, "%m/%d/%y", 1, t); break;
        case 'e': number(o, t->tm_mday, 2, ' '); break;
        case 'F': format(o, "%Y-%m-%d", 1, t); break;
        case 'g':
            iso_week(t, &iso_year);
            number(o, (iso_year % 100 + 100) % 100, 2, '0');
            break;
        case 'G':
            iso_week(t, &iso_year);
            number(o, iso_year, 1, '0');
            break;
        case 'H': number(o, t->tm_hour, 2, '0'); break;
        case 'I': number(o, hour12, 2, '0'); break;
        case 'j': number(o, t->tm_yday + 1, 3, '0'); break;
        case 'm': number(o, t->tm_mon + 1, 2, '0'); break;
        case 'M': number(o, t->tm_min, 2, '0'); break;
        case 'n': put(o, '\n'); break;
        case 'p': puts_n(o, t->tm_hour < 12 ? "AM" : "PM", 2); break;
        case 'r': format(o, "%I:%M:%S %p", 1, t); break;
        case 'R': format(o, "%H:%M", 1, t); break;
        case 'S': number(o, t->tm_sec, 2, '0'); break;
        case 't': put(o, '\t'); break;
        case 'T': format(o, "%H:%M:%S", 1, t); break;
        case 'u': number(o, (t->tm_wday + 6) % 7 + 1, 1, '0'); break;
        case 'U': number(o, (t->tm_yday + 7 - t->tm_wday) / 7, 2, '0'); break;
        case 'V': number(o, iso_week(t, &iso_year), 2, '0'); break;
        case 'w': number(o, t->tm_wday, 1, '0'); break;
        case 'W': number(o, (t->tm_yday + 7 - (t->tm_wday + 6) % 7) / 7, 2, '0'); break;
        case 'x': format(o, "%m/%d/%y", 1, t); break;
        case 'X': format(o, "%H:%M:%S", 1, t); break;
        case 'y': number(o, (year % 100 + 100) % 100, 2, '0'); break;
        case 'Y': number(o, year, 1, '0'); break;
        case 'z': puts_n(o, "+0000", 5); break;
        case 'Z': puts_n(o, "UTC", 3); break;
        case '%': put(o, '%'); break;
        case 0:
            fmt -= fs;          /* a % last: nothing to convert */
            break;
        default:
            put(o, '%');
            put(o, F(fmt));
        }
    }
#undef F
}

size_t strftime(char *s, size_t max, const char *fmt, const struct tm *t)
{
    Out o;

    o.to = s;
    o.max = max;
    o.n = 0;
    o.wide = 0;
    format(&o, fmt, 1, t);
    if (o.n >= max)
        return 0;               /* with its terminator, it did not fit */
    s[o.n] = 0;

    return o.n;
}

size_t wcsftime(wchar_t *s, size_t max, const wchar_t *fmt, const struct tm *t)
{
    Out o;

    o.to = s;
    o.max = max;
    o.n = 0;
    o.wide = 1;
    format(&o, (const char *) fmt, sizeof (wchar_t), t);
    if (o.n >= max)
        return 0;
    s[o.n] = 0;

    return o.n;
}

/* C99 7.23.3.1 gives this as a call to sprintf, and this is the same
 * text: the day, the month, the day of the month in three places, the time,
 * and the year. */
char *asctime(const struct tm *t)
{
    static char text[40];

    strftime(text, sizeof text, "%a %b %e %H:%M:%S %Y\n", t);

    return text;
}

char *ctime(const time_t *t)
{
    return asctime(localtime(t));
}
