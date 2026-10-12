# `time.h`

The time of day from the Agon's clock, the calendar, the time written
out, and a count of hundredths of a second for timing a program.

The header gives `time_t`, `clock_t`, `struct tm`, `CLOCKS_PER_SEC`,
`size_t` (an unsigned int, 24 bits) and `NULL`, and no other name of
`stddef.h`.

The Agon's clock keeps no time zone, so the time it keeps is taken as
UTC, and local time is the same: `localtime` is `gmtime`, `tm_isdst` is
always 0, and `strftime` writes the zone as `+0000` and `UTC`. The
calendar is the proleptic Gregorian one, for every year before or after
1970.

## `time_t`, `tm`

    typedef long long time_t;

    struct tm {
        int tm_sec;     /* 0 to 60 */
        int tm_min;     /* 0 to 59 */
        int tm_hour;    /* 0 to 23 */
        int tm_mday;    /* 1 to 31 */
        int tm_mon;     /* 0 to 11 */
        int tm_year;    /* since 1900 */
        int tm_wday;    /* 0 to 6, from Sunday */
        int tm_yday;    /* 0 to 365 */
        int tm_isdst;   /* always 0 */
    };

A `time_t` is a count of seconds since the start of 1970, UTC, as it is
everywhere else, and is 64 bits, so 2038 is not the end of it. Print one
with `%lld`. A `struct tm` is the same time as a date and a time of day;
its fields are `int`s, 24 bits.

## `time`

    time_t time(time_t *t);

The time now, from the Agon's real-time clock, which is the ESP32's on
the VDP. Each call asks MOS to fetch it from the VDP afresh, and reads
the date and time MOS leaves in its system variables; that year is a
signed byte counted from 1980. When `t` is not `NULL` the answer is
stored in `*t` too.

A clock that was never set reads as day 0 of its month, and then `time`
answers -1, as C says it does when the time is not available.

    time_t now = time(NULL);

    if (now == -1)
        puts("the clock is not set");
    else
        printf("%s", ctime(&now));

## `clock`, `clock_t`, `CLOCKS_PER_SEC`

    #define CLOCKS_PER_SEC 100UL

    typedef unsigned long clock_t;

    clock_t clock(void);

MOS's count of hundredths of a second since the machine was turned on,
which it advances by two each time the screen finishes a frame. It is
not the time the program has run, so time a piece of a program by the
difference of two calls:

    clock_t start = clock(), took;

    work();
    took = clock() - start;
    printf("%lu.%02lu s\n", took / CLOCKS_PER_SEC,
           took % CLOCKS_PER_SEC);

`CLOCKS_PER_SEC` is 100, the figure agondev uses too, and an integer, so
a program divides by it without floating point. The count is 32 bits,
and the difference of two `clock_t`s is right across its wrap.

MOS updates the four bytes of the count from an interrupt, and `clock`
reads them one at a time, so a read that falls in the middle of a carry
can see some bytes from before it and some from after.

## `difftime`

    double difftime(time_t t1, time_t t0);

`t1 - t0`, in seconds, as a `double`. A `double` is the same four-byte
type as a `float` on acc, so the answer is exact only up to 16777216
seconds (about 194 days); subtract the two `time_t`s, which are
integers, for a longer span to the second.

## `mktime`

    time_t mktime(struct tm *t);

The `time_t` for the date and time in `*t`, which are taken as UTC.
`tm_wday`, `tm_yday` and `tm_isdst` are not read.

The other fields need not be in range: each carries into the one above
it, and a negative one borrows from it. 61 seconds is a minute and a
second, month 14 is February of the next year, and day 0 is the last
day of the month before. `*t` is then rewritten with every field in
range, `tm_wday` and `tm_yday` filled in, and `tm_isdst` 0.

    struct tm t = *gmtime(&now);

    t.tm_mday += 30;                /* thirty days on */
    later = mktime(&t);

It never answers -1 for an error: every `struct tm` is some time.

`mktime` works through `gmtime`, so it writes over the `struct tm` that
`gmtime` and `localtime` answer.

## `gmtime`, `localtime`

    struct tm *gmtime(const time_t *t);
    struct tm *localtime(const time_t *t);

`*t` as a date and a time of day, UTC, with every field filled in and
`tm_isdst` 0. `localtime` is `gmtime`, since the Agon keeps no time zone.

Both answer the same static `struct tm`, which the next call of either,
of `mktime` or of `ctime`, writes over. They never answer `NULL`; a
time whose year does not fit in an `int` loses its top bits.

## `asctime`, `ctime`

    char *asctime(const struct tm *t);
    char *ctime(const time_t *t);

The time as C's fixed form for it, with a `'\n'` at the end:

    Thu Jan  1 00:00:00 1970

The day of the week and the month as three letters, the day of the
month in two places (a space in front of one digit), the time, and the
year. It is `strftime`'s `"%a %b %e %H:%M:%S %Y\n"`.

`ctime(t)` is `asctime(localtime(t))`. Both answer the same static
buffer, which the next call of either writes over.

## `strftime`

    size_t strftime(char *s, size_t max, const char *fmt,
                    const struct tm *t);

Write the time in `*t` into `s` as `fmt` says, as `printf` writes
numbers: characters other than `%` are copied, and a `%` and the
character after it are a conversion. The locale is C's, the only one
acc has. [`wcsftime`](wchar.md#wcsftime), in `wchar.h`, is the same
for wide strings.

| Conv | Writes                              | Example        |
|------|-------------------------------------|----------------|
| `%a` | the weekday, three letters          | `Thu`          |
| `%A` | the weekday                         | `Thursday`     |
| `%b` | the month, three letters            | `Jan`          |
| `%h` | the same as `%b`                    | `Jan`          |
| `%B` | the month                           | `January`      |
| `%c` | `%a %b %e %H:%M:%S %Y`              | see `asctime`  |
| `%C` | the century, two digits             | `19`           |
| `%d` | the day of the month, `01` to `31`  | `01`           |
| `%D` | `%m/%d/%y`                          | `01/01/70`     |
| `%e` | the day of the month, space-padded  | `" 1"`         |
| `%F` | `%Y-%m-%d`                          | `1970-01-01`   |
| `%g` | the ISO 8601 week's year, 2 digits  | `70`           |
| `%G` | the ISO 8601 week's year            | `1970`         |
| `%H` | the hour, `00` to `23`              | `00`           |
| `%I` | the hour, `01` to `12`              | `12`           |
| `%j` | the day of the year, `001` to `366` | `001`          |
| `%m` | the month, `01` to `12`             | `01`           |
| `%M` | the minute, `00` to `59`            | `00`           |
| `%n` | a `'\n'`                            |                |
| `%p` | `AM` or `PM`                        | `AM`           |
| `%r` | `%I:%M:%S %p`                       | `12:00:00 AM`  |
| `%R` | `%H:%M`                             | `00:00`        |
| `%S` | the second, `00` to `60`            | `00`           |
| `%t` | a `'\t'`                            |                |
| `%T` | `%H:%M:%S`                          | `00:00:00`     |
| `%u` | the weekday, `1` (Monday) to `7`    | `4`            |
| `%U` | the week, from the first Sunday     | `00`           |
| `%V` | the ISO 8601 week, `01` to `53`     | `01`           |
| `%w` | the weekday, `0` (Sunday) to `6`    | `4`            |
| `%W` | the week, from the first Monday     | `00`           |
| `%x` | `%m/%d/%y`                          | `01/01/70`     |
| `%X` | `%H:%M:%S`                          | `00:00:00`     |
| `%y` | the year, two digits                | `70`           |
| `%Y` | the year, all its digits            | `1970`         |
| `%z` | the offset from UTC, always         | `+0000`        |
| `%Z` | the zone's name, always             | `UTC`          |
| `%%` | a `%`                               | `%`            |

The examples are for 1970-01-01 00:00:00. `%U` and `%W` count the days
before the year's first Sunday (or Monday) as week 0. The ISO 8601 week
starts on a Monday, and week 1 is the one with the year's first
Thursday in it, so near New Year `%G` may be the year before or after
`%Y`.

C's `E` and `O` modifiers (`%Ey`, `%Od`) are accepted, and make no
difference, as they make none in the C locale. Any other character
after a `%` is written as it stands, with the `%`; a `%` at the end of
`fmt` writes nothing.

The fields are read as they are, not put in range first: pass a `struct
tm` that `gmtime` or `mktime` filled in. The names of weekdays and
months are taken from `tm_wday` and `tm_mon` modulo 7 and 12, so they
must not be negative.

The answer is the number of characters written, not counting the
terminator that follows them. When they and the
terminator do not fit in `max`, the answer is 0, and `s` holds the
first `max` characters with no terminator.

    char line[32];

    strftime(line, sizeof line, "%Y-%m-%d %H:%M", gmtime(&now));
