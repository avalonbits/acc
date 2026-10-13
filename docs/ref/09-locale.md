# `locale.h`

Locales: there is one, the C locale, which is what every program starts
in. The header gives `struct lconv`, `NULL`, and the `LC_` categories,
and the two functions that name the locale and describe it.

Nothing else in the library asks which locale is in force: `printf`
writes a point, `strtod` reads one, `ctype.h` classifies ASCII, and the
multibyte characters are UTF-8, whatever `setlocale` has been told.

## `setlocale`, `LC_ALL`, `LC_COLLATE`, `LC_CTYPE`, `LC_MONETARY`, `LC_NUMERIC`, `LC_TIME`

    char *setlocale(int category, const char *locale);

    #define LC_ALL      0
    #define LC_COLLATE  1
    #define LC_CTYPE    2
    #define LC_MONETARY 3
    #define LC_NUMERIC  4
    #define LC_TIME     5

Set the locale of `category` to `locale`, and answer the locale's
name. acc accepts two names, for any of the six categories: `"C"`, and
`""`, the machine's own locale, which on the Agon is the C locale too.
Either answers `"C"`. A `NULL` `locale` asks which locale is in force
and changes nothing; it also answers `"C"`.

Any other name, such as `"POSIX"`, `"C.UTF-8"` or `"en_US"`, is refused,
and so is a `category` that is none of the six: the answer is `NULL`,
and the locale stays as it was. The name answered is a literal, not to
be changed.

    setlocale(LC_ALL, "");          /* "C" */
    setlocale(LC_ALL, "de_DE");     /* NULL */

## `localeconv`, `lconv`

    struct lconv *localeconv(void);

The C locale's conventions for writing numbers and money, as C99 gives
them:

| Member                | Value      | Member                | Value      |
|-----------------------|------------|-----------------------|------------|
| `decimal_point`       | `"."`      | `frac_digits`         | `CHAR_MAX` |
| `thousands_sep`       | `""`       | `p_cs_precedes`       | `CHAR_MAX` |
| `grouping`            | `""`       | `n_cs_precedes`       | `CHAR_MAX` |
| `mon_decimal_point`   | `""`       | `p_sep_by_space`      | `CHAR_MAX` |
| `mon_thousands_sep`   | `""`       | `n_sep_by_space`      | `CHAR_MAX` |
| `mon_grouping`        | `""`       | `p_sign_posn`         | `CHAR_MAX` |
| `positive_sign`       | `""`       | `n_sign_posn`         | `CHAR_MAX` |
| `negative_sign`       | `""`       | the `int_` ones of    | `CHAR_MAX` |
| `currency_symbol`     | `""`       | those seven           |            |
| `int_curr_symbol`     | `""`       |                       |            |

An empty string, or `CHAR_MAX` (127, as a `char` is signed), is C's way
of saying the locale has no such thing: no grouping of digits, no
currency, no sign convention.

The answer is the same static structure every time. It is not `const`,
as C hands it out, so a program can write to it, and changes only what
it reads back: nothing in the library reads it.
