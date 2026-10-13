# `inttypes.h`

The `printf` and `scanf` conversions for the types of
[`stdint.h`](stdint.md), and functions over `intmax_t`, which is a
`long long` here.

The header includes `stdint.h`, so it gives all of that header's types
and macros too.

## `PRId32`, `PRIu32`, `PRIx32`, `PRId64`, `PRIu64`, `PRIx64`, `PRIu8`

The `printf` conversions, as strings to put after a `%`:

    uint32_t size = 100000;
    int16_t delta = -5;

    printf("%" PRIu32 " bytes, %" PRId16 "\n", size, delta);

Each name is `PRI`, a conversion letter (`d`, `i`, `o`, `u`, `x` or
`X`), and a type: a width for the exact type (`PRIu32`), `LEAST` or
`FAST` and the width (`PRIuLEAST32`, `PRIuFAST32`), `MAX` for `intmax_t`
(`PRIuMAX`), or `PTR` for `intptr_t` (`PRIuPTR`). The widths are 8, 16,
24, 32 and 64, the 24 only for the exact type; `PRId24` and its family
are not C99's.

The letter is the conversion; the type sets the length in front of it:

| Type of the macro                       | Length | `PRId...` is |
|-----------------------------------------|--------|--------------|
| 8 and 16 bits, exact, least and fast    | none   | `"d"`        |
| 24 bits, `PTR`                          | none   | `"d"`        |
| 32 bits, exact, least and fast          | `l`    | `"ld"`       |
| 64 bits, exact, least and fast, `MAX`   | `ll`   | `"lld"`      |

A value narrower than an `int` arrives at `printf` as an `int`, so the
8- and 16-bit ones have no length. That makes `PRIx8` and `PRIx16` write
a negative `int8_t` or `int16_t` as a 24-bit `unsigned int`: `-1` is
`ffffff`. Converting it to `uint8_t` or `uint16_t` first writes `ff` or
`ffff`.

See [`printf`](stdio.md#printf-fprintf-sprintf-snprintf) for the
conversions and lengths themselves.

## `SCNd32`, `SCNu32`, `SCNx32`, `SCNd64`, `SCNu64`, `SCNx64`, `SCNx8`

The `scanf` conversions, named the same way as the `PRI` ones with `SCN`
in front, for the letters `d`, `i`, `o`, `u` and `x` (C99 has no
`SCNX`):

    uint8_t red, green, blue;           /* from "ff8000" */

    if (sscanf(text, "%2" SCNx8 "%2" SCNx8 "%2" SCNx8,
               &red, &green, &blue) == 3)
        set_colour(red, green, blue);

`scanf` stores through a pointer, so it is told every width:

| Type of the macro                       | Length | `SCNd...` is |
|-----------------------------------------|--------|--------------|
| 8 bits, exact, least and fast           | `hh`   | `"hhd"`      |
| 16 bits, exact, least and fast          | `h`    | `"hd"`       |
| 24 bits, `PTR`                          | none   | `"d"`        |
| 32 bits, exact, least and fast          | `l`    | `"ld"`       |
| 64 bits, exact, least and fast, `MAX`   | `ll`   | `"lld"`      |

See [`scanf`](stdio.md#scanf-fscanf-sscanf) for how each reads.

## `PRIdMAX`, `PRIuMAX`, `PRIxMAX`, `PRIdPTR`, `PRIuPTR`, `PRIxPTR`

The conversions for `intmax_t` and `uintmax_t` (`"lld"`, `"llu"`,
`"llx"` and the others with `ll`), and for `intptr_t` and `uintptr_t`
(`"d"`, `"u"`, `"x"` and the others with no length, since those are
`int`s). `SCNdMAX`, `SCNuMAX`, `SCNdPTR` and the rest are the same
strings.

## `imaxabs`

    intmax_t imaxabs(intmax_t j);

The magnitude of `j`. `INTMAX_MIN` has no magnitude an `intmax_t` holds.

## `imaxdiv`, `imaxdiv_t`

    typedef struct {
        intmax_t quot;
        intmax_t rem;
    } imaxdiv_t;

    imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);

`numer / denom` in `quot` and `numer % denom` in `rem`, both at once,
as [`lldiv`](stdlib.md#div-ldiv-lldiv-div_t-ldiv_t-lldiv_t) has them:
the quotient is truncated toward zero, so `rem` has `numer`'s sign. It
is the two operators and nothing more, so `denom` must not be 0.

## `strtoimax`, `strtoumax`

    intmax_t  strtoimax(const char *s, char **end, int base);
    uintmax_t strtoumax(const char *s, char **end, int base);

`strtoll` and `strtoull` under the names of `intmax_t`: they call those
and answer what they answer. See
[`strtol`](stdlib.md#strtol-strtoul-strtoll-strtoull) for the bases,
`*end`, and `ERANGE` with `INTMAX_MIN`, `INTMAX_MAX` or `UINTMAX_MAX`
for a number past the range.

## `wcstoimax`, `wcstoumax`

    intmax_t  wcstoimax(const wchar_t *s, wchar_t **end, int base);
    uintmax_t wcstoumax(const wchar_t *s, wchar_t **end, int base);

`strtoimax` and `strtoumax` over a wide string: the same reader, a
`wchar_t` (16 bits) at a time. The spaces, sign, `0x` and digits are
the same characters as `strtol` takes; any other wide character ends the
number. `*end` is left on a `wchar_t` of `s`.

The header declares them without the name `wchar_t`, over a `short`
that is the same type, since C99 does not give `inttypes.h` that name.
A program that writes `wchar_t` itself includes `stddef.h` or `wchar.h`
for it.
