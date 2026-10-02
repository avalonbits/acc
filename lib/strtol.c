/*
 * strtol, strtoul, strtoll and strtoull: an integer at the front of a string.
 *
 * Copyright (C) 2026 Igor Cananea <icc@avalonbits.com>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * One reader for all four, and for the wide ones in <wchar.h> and the
 * intmax ones in <inttypes.h>: what differs between them is the largest
 * value each type holds and how wide a character is, so both are handed in.
 */
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

/* The magnitude of the number at `s`, and whether a minus sign came in
 * front of it.
 *
 * `step` is how wide a character is -- 1 for a char, 2 for a wchar_t -- so
 * that a wide string is read in place rather than copied: nothing past
 * ASCII can be part of a number, so the wide character's low byte and a
 * test that the high one is clear are all a digit needs. `pos` and `neg`
 * are the largest magnitudes the result may have with each sign. A number
 * past its limit reads to its end anyway, as C asks, and comes back as the
 * limit with ERANGE in errno.
 *
 * An unsigned conversion is handed the same limit for both signs, and C
 * says its minus sign negates the value in the unsigned type -- except
 * that a magnitude past the limit is the limit, sign or not. So when the
 * two limits are the same and the number went past them, the sign is
 * dropped: the caller then answers the limit, which is ULONG_MAX or
 * ULLONG_MAX, and not its negation.
 *
 * `end`, when it is wanted, is left on the first character not part of the
 * number, and on `s` itself when there was no number. "0x" with no hex
 * digit after it is the number 0 followed by an x. */
unsigned long long __acc_strtou(const char *s, char **end, int base, int step,
                                unsigned long long pos, unsigned long long neg,
                                int *negative)
{
    const char *start = s, *zero_at = 0;
    unsigned long long value = 0, limit, cut;
    int over = 0, cut_digit, c;

#define AT(p) (step == 1 ? *(const unsigned char *) (p) \
                         : *(const unsigned short *) (p))

    *negative = 0;
    while (AT(s) == ' ' || (AT(s) >= 9 && AT(s) <= 13))
        s += step;
    if (AT(s) == '+' || AT(s) == '-') {
        *negative = AT(s) == '-';
        s += step;
    }

    if ((base == 0 || base == 16) && AT(s) == '0'
        && (AT(s + step) == 'x' || AT(s + step) == 'X')) {
        zero_at = s + step;
        s += 2 * step;
        base = 16;
    } else if (base == 0) {
        base = AT(s) == '0' ? 8 : 10;
    }
    if (base < 2 || base > 36) {
        if (end)
            *end = (char *) start;
        *negative = 0;

        return 0;
    }

    limit = *negative ? neg : pos;
    cut = limit / (unsigned) base;
    cut_digit = (int) (limit % (unsigned) base);

    for (const char *first = s;; s += step) {
        c = AT(s);
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'a' && c <= 'z')
            c -= 'a' - 10;
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 10;
        else
            c = 99;
        if (c >= base) {
            if (s == first) {
                /* No digits: after a 0x, the 0 was the number. */
                s = zero_at ? zero_at : start;
            }
            break;
        }
        if (over || value > cut || (value == cut && c > cut_digit)) {
            over = 1;
            continue;
        }
        value = value * (unsigned) base + (unsigned) c;
    }
#undef AT

    if (end)
        *end = (char *) s;
    if (!over)
        return value;

    errno = ERANGE;
    if (pos == neg)
        *negative = 0;

    return limit;
}

long strtol(const char *s, char **end, int base)
{
    int negative;
    unsigned long v = (unsigned long) __acc_strtou(s, end, base, 1, LONG_MAX,
                                                   (unsigned long) LONG_MAX + 1,
                                                   &negative);

    return (long) (negative ? -v : v);
}

unsigned long strtoul(const char *s, char **end, int base)
{
    int negative;
    unsigned long v = (unsigned long) __acc_strtou(s, end, base, 1, ULONG_MAX,
                                                   ULONG_MAX, &negative);

    return negative ? -v : v;
}

long long strtoll(const char *s, char **end, int base)
{
    int negative;
    unsigned long long v = __acc_strtou(s, end, base, 1, LLONG_MAX,
                                        (unsigned long long) LLONG_MAX + 1,
                                        &negative);

    return (long long) (negative ? -v : v);
}

unsigned long long strtoull(const char *s, char **end, int base)
{
    int negative;
    unsigned long long v = __acc_strtou(s, end, base, 1, ULLONG_MAX,
                                        ULLONG_MAX, &negative);

    return negative ? -v : v;
}

/* OpenBSD's strtonum: see <stdlib.h>. strtoll reads the number, and says
 * with ERANGE where it is past even a long long's range. */
long long strtonum(const char *nptr, long long minval, long long maxval,
                   const char **errstr)
{
    const char *error = NULL;
    int was = errno, why = 0;
    long long value = 0;
    char *end;

    if (minval > maxval) {
        error = "invalid";
        why = EINVAL;
    } else {
        errno = 0;
        value = strtoll(nptr, &end, 10);
        if (end == nptr || *end) {
            error = "invalid";
            why = EINVAL;
        } else if ((errno == ERANGE && value < 0) || value < minval) {
            error = "too small";
            why = ERANGE;
        } else if (errno == ERANGE || value > maxval) {
            error = "too large";
            why = ERANGE;
        }
    }
    errno = error ? why : was;
    if (errstr)
        *errstr = error;

    return error ? 0 : value;
}
