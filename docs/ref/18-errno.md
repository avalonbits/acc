# `errno.h`

The number a library function leaves when it fails, and the names of
the numbers it leaves.

The header gives `errno` and four error numbers: C's `EDOM`, `ERANGE`
and `EILSEQ`, and POSIX's `EINVAL`. The numbers are Linux's, so a
number printed on the Agon means what it would mean anywhere else.
Nothing in the library sets `errno` to any other.

## `errno`

    extern int errno;
    #define errno errno

An `int` in the library's data, 0 when the program starts. A program
has one thread, so there is one `errno`, and the macro is the object
itself.

No function sets `errno` to 0. One that succeeds leaves it as it was,
so a program that wants to know whether a call failed sets it to 0
first and looks after:

    errno = 0;
    n = strtol(text, &end, 10);
    if (errno == ERANGE)
        puts("too large");

The file functions of `stdio.h` do not set `errno` (see
[`stdio.h`](04-stdio.md)), nor do `time.h`'s or `signal`. The words for a
number are [`strerror`](06-string.md#strerror)'s, and
[`perror`](04-stdio.md#perror) prints those for `errno`.

## `EDOM`

    #define EDOM 33

An argument outside the function's domain. The functions of `math.h`
set it, and answer a NaN, as C asks when
[`math_errhandling`](10-math.md#math_errhandling-math_errno-math_errexcept)
is `MATH_ERRNO`, which it is:

- `sqrt` of a negative number, `-INFINITY` among them;
- `log`, `log2` and `log10` of a negative number, and `log1p` below -1;
- `asin` and `acos` past 1 either way, `acosh` below 1 and `atanh`
  past 1 either way;
- `sin`, `cos` and `tan` of an infinity;
- `pow` of a negative number to a power that is not a whole number;
- `fmod`, `remainder` and `remquo` of an infinity, or by 0;
- `tgamma` of a negative whole number or of `-INFINITY`;
- `ilogb` of 0, of a NaN and of an infinity, which answers
  `FP_ILOGB0`, `FP_ILOGBNAN` or `INT_MAX` rather than a NaN.

The `float` forms (`sqrtf` and the rest) are the same functions.

## `ERANGE`

    #define ERANGE 34

A result too large for its type. `math.h`'s functions set it at a pole
or for a result past the largest float, and answer `HUGE_VAL`, which is
infinity, with the result's sign: `exp`, `exp2`, `expm1`, `pow`,
`sinh`, `cosh`, `hypot`, `scalbn`, `scalbln`, `ldexp`, `nextafter`,
`tgamma` and `lgamma` past the largest, and `log`, `log2`, `log10` of
0, `log1p` of -1, `atanh` of 1 or -1, `pow` of 0 to a negative power,
and `tgamma` and `lgamma` at their poles. A result too small is not an
error in `math.h`; it is the nearest number there is, a denormal or 0.

The functions that read numbers from strings set it too:

- [`strtol`](17-stdlib.md#strtol-strtoul-strtoll-strtoull), `strtoul`,
  `strtoll`, `strtoull`, `strtoimax` and `strtoumax`, and the `wcsto`
  forms of each, for a number past the type's range;
- [`strtod`](17-stdlib.md#strtod-strtof-atof), `strtof`, `atof`, `wcstod`
  and `wcstof`, for a number too large for a float, or too small to be
  held exactly;
- [`strtonum`](17-stdlib.md#strtonum), for a number outside the range it is
  given;
- the [`scanf`](04-stdio.md#scanf-fscanf-sscanf) family, for a number it
  reads that is out of range.

## `EILSEQ`

    #define EILSEQ 84

Bytes that are not a character: not UTF-8, or a character outside the
16 bits of a `wchar_t`, or a wide character that is one of the
surrogates. The multibyte and wide character conversions set it: of
`stdlib.h`, [`mblen`](17-stdlib.md#mblen-mbtowc-wctomb-mb_cur_max),
`mbtowc`, `wctomb`, [`mbstowcs`](17-stdlib.md#mbstowcs-wcstombs) and
`wcstombs`; of `wchar.h`, `mbrtowc`, `mbrlen`, `wcrtomb`, `mbsrtowcs`
and `wcsrtombs`; and `fgetwc` and `fputwc`, and so the wide reads and
writes made of them.

## `EINVAL`

    #define EINVAL 22

An argument that is not valid. [`strtonum`](17-stdlib.md#strtonum) sets
it for a string that is not a number, and for a `minval` greater than
`maxval`; [`signal`](20-signal.md#signal-sig_dfl-sig_ign-sig_err) and
[`raise`](20-signal.md#raise) for a signal there is no such thing as. It is
POSIX's, not C99's.
