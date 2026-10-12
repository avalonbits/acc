# `wctype.h`

What kind of character a wide character is, and its letter in the other
case: C99's twelve wide classes, `towlower` and `towupper`, and the
classes and mappings looked up by name. The header gives `wint_t` (an
`int`, 24 bits), `wctype_t`, `wctrans_t` (both `int`s) and `WEOF` (-1).

There is only the C locale, and its classes are ASCII's. Each function
here is the [`ctype.h`](ctype.md) one of the same name: a wide character
is in a class when it is an ASCII character in that class, and nothing
from 128 up, to 0xFFFF and past it, is in any class. `WEOF` is in none.
The mappings change the twenty-six letters and leave everything else
alone, `WEOF` included.

They are functions rather than macros, so a program can take their
address.

## `iswalpha`, `iswupper`, `iswlower`, `iswdigit`, `iswxdigit`, `iswalnum`

    int iswalpha(wint_t wc);
    int iswupper(wint_t wc);
    int iswlower(wint_t wc);
    int iswdigit(wint_t wc);
    int iswxdigit(wint_t wc);
    int iswalnum(wint_t wc);

Letters and digits, as
[`isalpha`](ctype.md#isalpha-isupper-islower-isdigit-isxdigit-isalnum)
and the others answer them: `iswupper` answers 1 for `A` to `Z`,
`iswlower` for `a` to `z`, `iswalpha` for either, `iswdigit` for `0` to
`9`, `iswxdigit` for those and `a` to `f` in either case, and
`iswalnum` for a letter or a digit. A letter past ASCII, such as 0xE9
(e with an acute accent), is not a letter here.

## `iswspace`, `iswblank`

    int iswspace(wint_t wc);
    int iswblank(wint_t wc);

White space, as [`isspace`](ctype.md#isspace-isblank) answers it:
`iswspace` answers 1 for a space (32) and for 9 to 13, and `iswblank`
for space and tab. No wide space past ASCII is white space.

## `iswcntrl`, `iswprint`, `iswgraph`, `iswpunct`

    int iswcntrl(wint_t wc);
    int iswprint(wint_t wc);
    int iswgraph(wint_t wc);
    int iswpunct(wint_t wc);

As [`iscntrl`](ctype.md#iscntrl-isprint-isgraph-ispunct) and the others:
`iswcntrl` answers 1 for 0 to 31 and 127, `iswprint` for 32 to 126,
`iswgraph` for 33 to 126, and `iswpunct` for what `iswgraph` takes and
`iswalnum` does not. A wide character from 128 up is none of them, not
even printable.

## `towlower`, `towupper`

    wint_t towlower(wint_t wc);
    wint_t towupper(wint_t wc);

`towlower` answers the lower-case letter for `A` to `Z`, and `towupper`
the upper-case one for `a` to `z`. Anything else comes back as it was.

## `wctype`, `iswctype`, `wctype_t`

    wctype_t wctype(const char *property);
    int iswctype(wint_t wc, wctype_t desc);

`wctype` answers the class named `property`, for `iswctype` to test:
`iswctype(wc, wctype("alpha"))` is `iswalpha(wc)`. The names are those
of the twelve functions without their `isw`:

| `property` | Value | Test       | `property` | Value | Test        |
|------------|-------|------------|------------|-------|-------------|
| `"alnum"`  | 1     | `iswalnum` | `"lower"`  | 7     | `iswlower`  |
| `"alpha"`  | 2     | `iswalpha` | `"print"`  | 8     | `iswprint`  |
| `"blank"`  | 3     | `iswblank` | `"punct"`  | 9     | `iswpunct`  |
| `"cntrl"`  | 4     | `iswcntrl` | `"space"`  | 10    | `iswspace`  |
| `"digit"`  | 5     | `iswdigit` | `"upper"`  | 11    | `iswupper`  |
| `"graph"`  | 6     | `iswgraph` | `"xdigit"` | 12    | `iswxdigit` |

The C locale has no other classes. A name not in the table answers 0,
which is no class: `iswctype` answers 0 for it, as for any other
`desc` not in the table.

## `wctrans`, `towctrans`, `wctrans_t`

    wctrans_t wctrans(const char *property);
    wint_t towctrans(wint_t wc, wctrans_t desc);

`wctrans` answers the mapping named `property`, for `towctrans` to
apply: `"tolower"` (1), which is `towlower`, and `"toupper"` (2), which
is `towupper`. Any other name answers 0, which is no mapping, and
`towctrans` with a `desc` that is neither 1 nor 2 answers `wc` as it
was.
