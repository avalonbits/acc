/*
 * <time.h>: the clock MOS keeps, the calendar, and the time of day.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The calendar is the proleptic Gregorian one, in whole days from
 * 1970-01-01, by Howard Hinnant's days_from_civil and civil_from_days,
 * which need no table and no loop over the years.
 */

#include <time.h>

#include <agon/mos.h>

int acc_rt_mos(int fn, int hl, int de, int bc);

/* The count of centiseconds since the machine was turned on, which MOS
 * adds two to every time the screen finishes a frame.
 *
 * Read as four separate bytes rather than as a long, because the four are
 * not written at once: MOS updates them from its own interrupt, and a read
 * that happened to fall in the middle of a carry would see the low byte
 * after it wrapped and the high one before. Reading them one at a time does
 * not fix that, and nothing here can -- what it does is keep the answer to
 * within a tick, which is what the clock is worth anyway. */
clock_t clock(void)
{
    uint8_t *v = mos_sysvars();

    return (clock_t) v[0] | ((clock_t) v[1] << 8) | ((clock_t) v[2] << 16)
           | ((clock_t) v[3] << 24);
}

double difftime(time_t t1, time_t t0)
{
    return (double) (t1 - t0);
}

static long long days_from_civil(long long y, int m, int d)
{
    long long era;
    int yoe, doy, doe;

    y -= m <= 2;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = (int) (y - era * 400);
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

    return era * 146097 + doe - 719468;
}

/* x / n and x % n, rounding down, so that a negative field borrows from
 * the one above it rather than going below zero. */
static long long carry(long long *x, long long n)
{
    long long q = *x / n;

    if (*x % n < 0)
        q--;
    *x -= q * n;

    return q;
}

/* The fields put back in range -- 61 seconds is a minute and a second,
 * month 14 is February of the next year -- and the time they come to. */
time_t mktime(struct tm *t)
{
    long long sec = t->tm_sec, min = t->tm_min, hour = t->tm_hour;
    long long mon = t->tm_mon, year = t->tm_year, days;

    min += carry(&sec, 60);
    hour += carry(&min, 60);
    days = carry(&hour, 24);
    year += carry(&mon, 12);
    days += days_from_civil(year + 1900, (int) mon + 1, 1) + t->tm_mday - 1;

    {
        time_t when = days * 86400 + hour * 3600 + min * 60 + sec;
        struct tm *n = gmtime(&when);

        *t = *n;

        return when;
    }
}

static struct tm broken;

struct tm *gmtime(const time_t *tp)
{
    long long z = *tp, secs, era, y;
    int doe, yoe, doy, mp, d, m;

    secs = z;
    z = carry(&secs, 86400);
    broken.tm_hour = (int) (secs / 3600);
    broken.tm_min = (int) (secs / 60 % 60);
    broken.tm_sec = (int) (secs % 60);
    broken.tm_wday = (int) ((z % 7 + 11) % 7);  /* 1970-01-01 was a Thursday */

    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (int) (z - era * 146097);
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y += m <= 2;

    broken.tm_year = (int) (y - 1900);
    broken.tm_mon = m - 1;
    broken.tm_mday = d;
    broken.tm_yday = (int) (days_from_civil(y, m, d) - days_from_civil(y, 1, 1));
    broken.tm_isdst = 0;

    return &broken;
}

struct tm *localtime(const time_t *t)
{
    return gmtime(t);
}

/* The Agon's clock, which is the ESP32's on the VDP: MOS asks it for the
 * time and leaves six bytes in its system variables, packed as MOS's
 * clock.c packs them. A clock that was never set says day 0, and then the
 * time is not available, which C says time answers with -1. */
time_t time(time_t *tp)
{
    static char text[64];
    uint8_t *v;
    unsigned long d;
    struct tm t;
    time_t when;

    acc_rt_mos(0x12, (int) text, 0, 0);         /* mos_getrtc: refresh */
    v = mos_sysvars() + sysvar_rtc;
    d = (unsigned long) v[0] | (unsigned long) v[1] << 8
        | (unsigned long) v[2] << 16 | (unsigned long) v[3] << 24;
    t.tm_mon = (int) (d & 0x0f);
    t.tm_mday = (int) ((d >> 4) & 0x1f);
    t.tm_hour = (int) ((d >> 21) & 0x1f);
    t.tm_min = (int) ((d >> 26) & 0x3f);
    t.tm_sec = v[4];
    t.tm_year = 80 + (signed char) v[5];
    when = t.tm_mday ? mktime(&t) : (time_t) -1;
    if (tp)
        *tp = when;

    return when;
}
