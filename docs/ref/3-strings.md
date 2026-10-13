# Strings and characters

Strings and blocks of memory, classifying characters narrow and wide, and the locale.

## `string.h`

Strings and blocks of memory: C99's `mem` and `str` functions, and the
extensions every C library has (`strdup`, `strndup`, `strnlen`, `stpcpy`,
`stpncpy`, `strcasecmp`, `strncasecmp` and `strchrnul`), which agondev
has too.

The header also gives `size_t` (an unsigned int, 24 bits) and `NULL`, and
no other name of `stddef.h`.

Bytes are compared as `unsigned char`, as C says. Most of these are
written in eZ80 assembly with the block instructions (`ldir`, `cpir`,
`cpi`), so a long string costs a few cycles a byte. A count of 0 is
always safe: nothing is read or written.

### `memcpy`, `memmove`

    void *memcpy(void *to, const void *from, size_t n);
    void *memmove(void *to, const void *from, size_t n);

Copy `n` bytes from `from` to `to`, and answer `to`.

`memmove` is right when the two overlap: it copies backwards when `to` is
inside the `n` bytes from `from`, and forwards otherwise. `memcpy` always
copies forwards, so with an overlap it is only right when `to` is below
`from`.

Where a program calls `memcpy`, `memmove` or `memset` by name, acc calls
its runtime's own versions, with the operands in registers; these library
ones are what a call through a pointer reaches.

### `memset`

    void *memset(void *s, int c, size_t n);

Fill `n` bytes from `s` with `c` (as an `unsigned char`), and answer `s`.

### `memcmp`

    int memcmp(const void *a, const void *b, size_t n);

Compare `n` bytes. The answer is 0 when they are all the same, and
otherwise the difference of the first two that differ: negative when
`a`'s is the smaller.

### `memchr`

    void *memchr(const void *s, int c, size_t n);

The first of the `n` bytes from `s` that is `c` (as an `unsigned char`),
or `NULL` when none is. It reads no further than the byte it finds.

### `strlen`, `strnlen`

    size_t strlen(const char *s);
    size_t strnlen(const char *s, size_t maxlen);

The length of `s`, not counting its terminator.

`strnlen` answers at most `maxlen`, and reads no further than
`s[maxlen - 1]`, so `s` need not have a terminator inside them. It is
POSIX's, not C99's.

### `strcpy`, `strcat`, `stpcpy`

    char *strcpy(char *to, const char *from);
    char *strcat(char *to, const char *from);
    char *stpcpy(char *to, const char *from);

`strcpy` copies `from` and its terminator to `to`; `strcat` copies them
to the end of the string at `to`. Both answer `to`.

`stpcpy` is `strcpy` answering where the terminator went, which is where
the next `stpcpy` of a string being built goes. It is POSIX's, not
C99's.

    char buf[40], *end = buf;

    end = stpcpy(end, "acc ");
    end = stpcpy(end, version);

`to` must have room for the whole of `from`; nothing checks.

### `strncpy`, `strncat`, `stpncpy`

    char *strncpy(char *to, const char *from, size_t n);
    char *strncat(char *to, const char *from, size_t n);
    char *stpncpy(char *to, const char *from, size_t n);

`strncpy` writes exactly `n` bytes to `to`: `from`, and then zeros to
make up `n`. A `from` of `n` characters or more is cut at `n` and left
**without a terminator**. It answers `to`.

`strncat` copies at most `n` characters of `from` to the end of the
string at `to`, and then always a terminator, so `to` needs room for
`n + 1` more. It answers `to`.

`stpncpy` is `strncpy` answering where it stopped copying: the first
zero it wrote, or `to + n` when it wrote none. It is POSIX's, not C99's.

### `strcmp`, `strncmp`

    int strcmp(const char *a, const char *b);
    int strncmp(const char *a, const char *b, size_t n);

Compare two strings, `strncmp` at most their first `n` characters. The
answer is 0 when they are the same, and otherwise the difference of the
first two characters that differ, as `unsigned char`: negative when `a`
comes first. A string that is the start of the other comes first.

### `strcasecmp`, `strncasecmp`

    int strcasecmp(const char *a, const char *b);
    int strncasecmp(const char *a, const char *b, size_t n);

`strcmp` and `strncmp` with the case of letters taken out: `'A'` to `'Z'`
are compared as `'a'` to `'z'`, as `tolower` has them in the C locale,
and no other character changes. Where the strings differ, the answer is
the difference of the two characters lowered. Both are POSIX's, not
C99's, and acc declares them in `string.h` (POSIX puts them in
`strings.h`, which acc does not have).

### `strchr`, `strrchr`, `strchrnul`

    char *strchr(const char *s, int c);
    char *strrchr(const char *s, int c);
    char *strchrnul(const char *s, int c);

`strchr` answers where `c` (as a `char`) first is in `s`, and `strrchr`
where it last is; both answer `NULL` when it is not there. The
terminator counts as part of the string, so a `c` of 0 finds the end.

`strchrnul` is `strchr` answering the end of `s`, its terminator, rather
than `NULL` when `c` is not there. It is GNU's, not C99's.

### `strstr`

    char *strstr(const char *hay, const char *needle);

Where `needle` first is in `hay`, or `NULL` when it is not there. An
empty `needle` is found at `hay`.

### `strspn`, `strcspn`, `strpbrk`

    size_t strspn(const char *s, const char *accept);
    size_t strcspn(const char *s, const char *reject);
    char *strpbrk(const char *s, const char *accept);

`strspn` counts how many of `s`'s first characters are all in `accept`,
and `strcspn` how many are all not in `reject`. `strpbrk` answers where
the first character of `s` that is in `accept` is, or `NULL` when none
is.

    s += strspn(s, " \t");          /* past the blanks */
    word = strcspn(s, " \t");       /* the length of the next word */

### `strtok`

    char *strtok(char *s, const char *delim);

Split a string into tokens separated by any of the characters in
`delim`. The first call names the string; each call after it passes
`NULL` and goes on from where the last stopped. Each call skips the
delimiters in front of the next token, writes a terminator over the
delimiter after it, and answers the token; at the end of the string it
answers `NULL`.

    char line[] = "ld a, (hl)";

    for (char *word = strtok(line, " ,"); word; word = strtok(NULL, " ,"))
        puts(word);                 /* ld, a, (hl) */

It changes the string, so the string cannot be a literal. It keeps where
it stopped in a static variable, so only one string can be split at a
time: a loop that calls a function that also uses `strtok` loses its
place.

### `strcoll`, `strxfrm`

    int strcoll(const char *a, const char *b);
    size_t strxfrm(char *to, const char *from, size_t n);

The locale's collating order. acc has only the C locale, where the order
is the bytes' own, so `strcoll` is `strcmp`.

`strxfrm` copies `from` and its terminator to `to` when it is shorter
than `n`, and copies nothing otherwise; either way it answers `from`'s
length. A call with an `n` of 0 (and `to` `NULL`) measures the room
needed, less one.

### `strdup`, `strndup`

    char *strdup(const char *s);
    char *strndup(const char *s, size_t n);

A copy of `s` in room from `malloc`, to be given back with `free`.
`strndup` copies at most `n` characters of `s` (which need not have a
terminator inside them) and always adds one. Both answer `NULL`, and
copy nothing, when `malloc` has no room. They are POSIX's, not C99's.

### `strerror`

    char *strerror(int n);

The words for the error number `n`, as `errno` holds it. They are
glibc's, so a message reads the same on the Agon as where the program
was written:

| `n`      | Words                                              |
|----------|----------------------------------------------------|
| 0        | Success                                            |
| `EINVAL` | Invalid argument                                   |
| `EDOM`   | Numerical argument out of domain                   |
| `ERANGE` | Numerical result out of range                      |
| `EILSEQ` | Invalid or incomplete multibyte or wide character  |

Any other number is `Unknown error n`, written into a static buffer
that the next such call writes over. The string is not to be changed.

`perror`, in `stdio.h`, prints the words for `errno` to `stderr`.

## `ctype.h`

What kind of character a byte is, and its letter in the other case:
C99's twelve classes and `tolower` and `toupper`, in the C locale, which
is the only one acc has. The classes are ASCII's: the letters are the
twenty-six either way, and nothing above 127 is in any class.

| Function   | Characters it answers 1 for                          |
|------------|------------------------------------------------------|
| `isdigit`  | `0` to `9`                                           |
| `isxdigit` | `0` to `9`, `a` to `f`, `A` to `F`                   |
| `islower`  | `a` to `z`                                           |
| `isupper`  | `A` to `Z`                                           |
| `isalpha`  | `a` to `z`, `A` to `Z`                               |
| `isalnum`  | `0` to `9`, `a` to `z`, `A` to `Z`                   |
| `isblank`  | space (32), tab (9)                                  |
| `isspace`  | space, tab, newline, vt, ff, cr (32, 9 to 13)        |
| `iscntrl`  | 0 to 31, and 127                                     |
| `isprint`  | 32 to 126: space and everything `isgraph` takes      |
| `isgraph`  | 33 to 126: `!` to `~`                                |
| `ispunct`  | what `isgraph` takes, less what `isalnum` takes      |

Each takes an `int`, which C99 says is a value that fits in an
`unsigned char`, or `EOF` (-1). Each answers 1 or 0, and 0 for `EOF`,
for every value from 128 to 255, and for any other number outside the
256 bytes, negative or past 255: they are comparisons rather than a
table, so no argument reads past anything. `tolower` and `toupper` give
back any of those unchanged.

A `char` is signed in acc, so a plain `char` past 127 arrives as a
negative number. That answers 0 here, and is still undefined in C; the
fix is the cast at the call:

    if (isalpha((unsigned char) *s))

They are functions rather than macros, so a program can take their
address. The Agon's screen font has characters above 127, but no class
here has any of them; a program that wants to sort those needs a table
of its own.

`wctype.h`'s `isw` functions, `towlower` and `towupper` are these under
wide names.

### `isalpha`, `isupper`, `islower`, `isdigit`, `isxdigit`, `isalnum`

    int isalpha(int c);
    int isupper(int c);
    int islower(int c);
    int isdigit(int c);
    int isxdigit(int c);
    int isalnum(int c);

Letters and digits. `isupper` answers 1 for `A` to `Z`, `islower` for
`a` to `z`, and `isalpha` for either. `isdigit` answers 1 for the ten
decimal digits `0` to `9`, `isxdigit` for those and the six hex letters
in either case, and `isalnum` for a letter or a decimal digit.

### `isspace`, `isblank`

    int isspace(int c);
    int isblank(int c);

White space. `isspace` answers 1 for a space (32) and for the five
characters from 9 to 13: tab, newline, vertical tab, form feed and
carriage return. `isblank` answers 1 for only the two that separate
words on a line, space and tab.

### `iscntrl`, `isprint`, `isgraph`, `ispunct`

    int iscntrl(int c);
    int isprint(int c);
    int isgraph(int c);
    int ispunct(int c);

`iscntrl` answers 1 for the control characters, 0 to 31 and 127 (DEL).
`isprint` answers 1 for the rest of ASCII, 32 to 126, and `isgraph` for
those but the space, 33 (`!`) to 126 (`~`). Every byte from 0 to 127 is
one of a control or a printing character, and no byte is both.

`ispunct` answers 1 for a character `isgraph` takes and `isalnum` does
not: the 32 of `!"#$%&'()*+,-./`, `:;<=>?@`, `[\]^_` and `` ` ``, and
`{|}~`.

### `tolower`, `toupper`

    int tolower(int c);
    int toupper(int c);

`tolower` answers the lower-case letter for `A` to `Z`, and `toupper`
the upper-case one for `a` to `z`. Anything else comes back as it was,
`EOF` and the values from 128 to 255 included.

    for (char *p = name; *p; p++)
        *p = toupper((unsigned char) *p);

## `wctype.h`

What kind of character a wide character is, and its letter in the other
case: C99's twelve wide classes, `towlower` and `towupper`, and the
classes and mappings looked up by name. The header gives `wint_t` (an
`int`, 24 bits), `wctype_t`, `wctrans_t` (both `int`s) and `WEOF` (-1).

There is only the C locale, and its classes are ASCII's. Each function
here is the [`ctype.h`](#ctypeh) one of the same name: a wide character
is in a class when it is an ASCII character in that class, and nothing
from 128 up, to 0xFFFF and past it, is in any class. `WEOF` is in none.
The mappings change the twenty-six letters and leave everything else
alone, `WEOF` included.

They are functions rather than macros, so a program can take their
address.

### `iswalpha`, `iswupper`, `iswlower`, `iswdigit`, `iswxdigit`, `iswalnum`

    int iswalpha(wint_t wc);
    int iswupper(wint_t wc);
    int iswlower(wint_t wc);
    int iswdigit(wint_t wc);
    int iswxdigit(wint_t wc);
    int iswalnum(wint_t wc);

Letters and digits, as
[`isalpha`](#isalpha-isupper-islower-isdigit-isxdigit-isalnum)
and the others answer them: `iswupper` answers 1 for `A` to `Z`,
`iswlower` for `a` to `z`, `iswalpha` for either, `iswdigit` for `0` to
`9`, `iswxdigit` for those and `a` to `f` in either case, and
`iswalnum` for a letter or a digit. A letter past ASCII, such as 0xE9
(e with an acute accent), is not a letter here.

### `iswspace`, `iswblank`

    int iswspace(wint_t wc);
    int iswblank(wint_t wc);

White space, as [`isspace`](#isspace-isblank) answers it:
`iswspace` answers 1 for a space (32) and for 9 to 13, and `iswblank`
for space and tab. No wide space past ASCII is white space.

### `iswcntrl`, `iswprint`, `iswgraph`, `iswpunct`

    int iswcntrl(wint_t wc);
    int iswprint(wint_t wc);
    int iswgraph(wint_t wc);
    int iswpunct(wint_t wc);

As [`iscntrl`](#iscntrl-isprint-isgraph-ispunct) and the others:
`iswcntrl` answers 1 for 0 to 31 and 127, `iswprint` for 32 to 126,
`iswgraph` for 33 to 126, and `iswpunct` for what `iswgraph` takes and
`iswalnum` does not. A wide character from 128 up is none of them, not
even printable.

### `towlower`, `towupper`

    wint_t towlower(wint_t wc);
    wint_t towupper(wint_t wc);

`towlower` answers the lower-case letter for `A` to `Z`, and `towupper`
the upper-case one for `a` to `z`. Anything else comes back as it was.

### `wctype`, `iswctype`, `wctype_t`

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

### `wctrans`, `towctrans`, `wctrans_t`

    wctrans_t wctrans(const char *property);
    wint_t towctrans(wint_t wc, wctrans_t desc);

`wctrans` answers the mapping named `property`, for `towctrans` to
apply: `"tolower"` (1), which is `towlower`, and `"toupper"` (2), which
is `towupper`. Any other name answers 0, which is no mapping, and
`towctrans` with a `desc` that is neither 1 nor 2 answers `wc` as it
was.

## `locale.h`

Locales: there is one, the C locale, which is what every program starts
in. The header gives `struct lconv`, `NULL`, and the `LC_` categories,
and the two functions that name the locale and describe it.

Nothing else in the library asks which locale is in force: `printf`
writes a point, `strtod` reads one, `ctype.h` classifies ASCII, and the
multibyte characters are UTF-8, whatever `setlocale` has been told.

### `setlocale`, `LC_ALL`, `LC_COLLATE`, `LC_CTYPE`, `LC_MONETARY`, `LC_NUMERIC`, `LC_TIME`

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

### `localeconv`, `lconv`

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
