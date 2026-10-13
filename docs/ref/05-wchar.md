# `wchar.h`

Wide characters and multibyte ones: wide strings, wide characters on
streams, the wide `printf` and `scanf`, numbers read from wide strings,
and the conversions between UTF-8 and wide characters. C99's
`wchar.h`, less `wcstold`, since acc has no `long double`.

The header gives `wchar_t` (a `short`, 16 bits), `wint_t` (an `int`, 24
bits), `mbstate_t`, `size_t` (an unsigned int, 24 bits), `NULL`, and
the macros `WEOF`, `WCHAR_MIN` and `WCHAR_MAX`. The functions that take
a stream take a `FILE *`, but the name `FILE`, and `stdin` and
`stdout`, come from `stdio.h`, which a program that uses them includes
too.

There is one locale, the C locale, and its multibyte characters are
UTF-8, as far as a 16-bit `wchar_t` goes: one byte below 0x80, two
below 0x800, and three for the rest up to 0xFFFF. A character UTF-8
writes in four bytes is past what a `wchar_t` holds, and is refused.
That is what acc makes of a `\u` in a narrow string literal too.

## `wchar_t`, `wint_t`, `WEOF`, `WCHAR_MIN`, `WCHAR_MAX`

    typedef short wchar_t;
    typedef int   wint_t;

    #define WEOF      ((wint_t) -1)
    #define WCHAR_MIN (-32768)
    #define WCHAR_MAX 32767

A `wchar_t` is signed, so the characters 0x8000 to 0xFFFF are held in
it as negative numbers, from -32768 to -1. A `wint_t` is wider, and the
functions that answer one (`fgetwc`, `fputwc`) answer a character as
0 to 0xFFFF, never negative, so that `WEOF` (-1) is none of them.

A wide string is an array of `wchar_t` ending in a 0; `L"..."` makes
one. The wide string functions count, and their `n`s are, wide
characters, not bytes.

## `mbstate_t`, `mbsinit`

    int mbsinit(const mbstate_t *ps);

A `mbstate_t` holds where a conversion has got to inside a character:
the value so far, and how many more bytes it wants. All zeros is the
initial state, between characters:

    mbstate_t st = { 0, 0, 0 };

UTF-8 has no shift states, so a `mbstate_t` matters only when a
character's bytes come in more than one call, as `mbrtowc` takes them.

`mbsinit` answers nonzero when `*ps` is between characters, or `ps` is
`NULL`, and 0 when it is in the middle of one.

## `mbrtowc`, `mbrlen`

    size_t mbrtowc(wchar_t *pwc, const char *s, size_t n, mbstate_t *ps);
    size_t mbrlen(const char *s, size_t n, mbstate_t *ps);

`mbrtowc` reads the character at `s`, looking at no more than `n`
bytes, and goes on from the state in `*ps`. When it has the whole
character, it puts it in `*pwc` (when `pwc` is not `NULL`) and answers
how many bytes it took in this call, or 0 when the character is the
null one.

| Answer         | Meaning                                         |
|----------------|-------------------------------------------------|
| 1 to 3         | the bytes of a whole character it took          |
| 0              | the character was the null one                  |
| `(size_t) -2`  | all `n` bytes taken, and the character is not   |
|                | finished: `*ps` holds it for the next call      |
| `(size_t) -1`  | not a character; `errno` is `EILSEQ`            |

Not a character is: a byte that cannot start one (0x80 to 0xC1, and
0xF0 to 0xFF, which start four bytes or more), a byte that cannot
continue one, a three-byte character below 0x800 (written with more
bytes than it needs), and the surrogates 0xD800 to 0xDFFF, which are
halves of characters. The last two are found at the character's last
byte, as glibc finds them. After `(size_t) -1`, `*ps` is back between
characters.

An `n` of 0 answers `(size_t) -2`. A `NULL` `s` is `mbrtowc(NULL, "",
1, ps)`: between characters it answers 0, and in the middle of one it
answers `(size_t) -1`, `EILSEQ`. A `NULL` `ps` uses a state of
`mbrtowc`'s own.

    mbstate_t st = { 0, 0, 0 };
    wchar_t wc;

    mbrtowc(&wc, "\xe2", 1, &st);       /* (size_t) -2 */
    mbrtowc(&wc, "\x82\xac", 2, &st);   /* 2, and wc is 0x20AC */

`mbrlen` is `mbrtowc` without `pwc`, with a state of its own when `ps`
is `NULL`.

## `wcrtomb`

    size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps);

Write `wc`'s UTF-8 bytes to `s`, which needs room for three, and answer
how many it wrote. A `wchar_t` below 0 is one of the characters 0x8000
to 0xFFFF. A surrogate, 0xD800 to 0xDFFF, is not a character: nothing
is written, the answer is `(size_t) -1`, and `errno` is `EILSEQ`.

A `NULL` `s` is a null `wc` written to a buffer of its own, and the
answer is 1. `*ps` is left between characters every time; a `NULL` `ps` uses a
state of `wcrtomb`'s own.

## `mbsrtowcs`, `wcsrtombs`

    size_t mbsrtowcs(wchar_t *dst, const char **src, size_t len,
                     mbstate_t *ps);
    size_t wcsrtombs(char *dst, const wchar_t **src, size_t len,
                     mbstate_t *ps);

`mbsrtowcs` converts the UTF-8 string at `*src` to wide characters at
`dst`, at most `len` of them, and answers how many it wrote, not
counting the terminator. When it gets to the terminator, it writes a 0
and sets `*src` to `NULL`; when it stops because `len` are written, it
sets `*src` past the last character it converted, so the next call goes
on from there. The terminator is written only when there is room for it
within the `len`.

`wcsrtombs` converts the wide string at `*src` to UTF-8 at `dst`, at
most `len` bytes, and never part of a character: when the next one does
not fit whole, it stops, with `*src` on that character. It answers how
many bytes it wrote, not counting the terminator, which again is
written, and `*src` set to `NULL`, only when there is room for it.

With `dst` `NULL`, both write nothing, ignore `len`, leave `*src` as it
was, and answer how many there would be: the room to allocate, less
one for the terminator.

At something that is not a character, as `mbrtowc` and `wcrtomb` judge
it, either answers `(size_t) -1` with `EILSEQ` in `errno`, and `*src`
(when `dst` is not `NULL`) is left on it. A `NULL` `ps` uses a state of
each one's own.

`mbstowcs` and `wcstombs` in
[`stdlib.h`](17-stdlib.md#mbstowcs-wcstombs) are these with a state of
their own each time.

## `btowc`, `wctob`

    wint_t btowc(int c);
    int wctob(wint_t c);

`btowc` answers the wide character for the byte `c` when it is a
character on its own, which in UTF-8 is 0 to 127; for any other value,
`EOF` included, it answers `WEOF`. `wctob` is the other way: `c` itself
when it is 0 to 127, and `EOF` for anything else.

## `fwide`

    int fwide(FILE *f, int mode);

A stream's orientation: the first function used on it makes it a byte
stream (one of `stdio.h`'s) or a wide one (one of these). `fwide` with a
`mode` above 0 makes a stream that has none wide, and one below 0 makes
it a byte stream; a `mode` of 0, or a stream that already has one,
changes nothing. The answer is the orientation after: above 0 for wide,
below 0 for bytes, 0 for none yet. `fwide(NULL, mode)` answers 0.

`fopen` and `freopen` start a stream with none. Neither kind of
function refuses a stream of the other kind: a wide character is its
UTF-8 bytes in the stream, and the byte functions read and write those
bytes as they are.

## `fgetwc`, `getwc`, `getwchar`

    wint_t fgetwc(FILE *f);
    wint_t getwc(FILE *f);
    wint_t getwchar(void);

The next character of `f` (of `stdin` for `getwchar`), read as its UTF-8
bytes through `fgetc`, as a `wint_t` from 0 to 0xFFFF, or `WEOF` at the
end of the input or on a failure.

Bytes that are not a character, or the end of the input in the middle
of one, answer `WEOF`, set the stream's error flag and put `EILSEQ` in
`errno`. The bytes read up to there are gone.

`getwc` is a function, not a macro, so it is `fgetwc` exactly. The
keyboard is read a line at a time, as
[`fgetc`](04-stdio.md#fgetc-getc-getchar) reads it.

## `ungetwc`

    wint_t ungetwc(wint_t c, FILE *f);

Push `c` back onto `f`, to be the next character `fgetwc` reads, and
take off its end-of-file flag. The answer is `c`.

The wide character is kept as itself, apart from the byte `ungetc`
keeps, since its bytes may be more than one. One can be pushed back: a
second `ungetwc` before the first is read answers `WEOF` and pushes
nothing, and so does a `c` of `WEOF` or a `NULL` `f`. Only the wide
functions read it back; `fgetc` does not see it. A seek, `fflush` or
`rewind` forgets it.

## `fgetws`

    wchar_t *fgetws(wchar_t *s, int n, FILE *f);

[`fgets`](04-stdio.md#fgets-gets) for wide characters: it reads characters
of `f` into `s` until it has read a `L'\n'` (which it keeps), has read
`n - 1`, or comes to the end of the input, and adds a terminator. The
answer is `s`, or `NULL` when nothing was read, or when the reading
ended on a failure with the error flag set, as bytes that are not a
character set it. An `n` of 1 or less answers `NULL`.

## `fputwc`, `putwc`, `putwchar`

    wint_t fputwc(wchar_t c, FILE *f);
    wint_t putwc(wchar_t c, FILE *f);
    wint_t putwchar(wchar_t c);

Write `c` as its UTF-8 bytes, through `fputc`, to `f` (to `stdout` for
`putwchar`). The answer is the character, as a `wint_t` from 0 to
0xFFFF, or `WEOF` when a byte could not be written, or when `c` is a
surrogate, which also sets the stream's error flag and puts `EILSEQ` in
`errno`.

`putwc` is a function, not a macro. The screen is sent the bytes as
they are, as a file is.

## `fputws`

    int fputws(const wchar_t *s, FILE *f);

Write the wide string `s`, without its terminator, to `f`, a character
at a time with `fputwc`. The answer is 0, or `EOF` at the first
character that could not be written.

## `wprintf`, `fwprintf`, `swprintf`

    int wprintf(const wchar_t *fmt, ...);
    int fwprintf(FILE *f, const wchar_t *fmt, ...);
    int swprintf(wchar_t *s, size_t n, const wchar_t *fmt, ...);

[`printf`](04-stdio.md#printf-fprintf-sprintf-snprintf) with a wide
format, writing wide characters: `wprintf` to `stdout`, `fwprintf` to
`f` (each character through `fputwc`, so as UTF-8), and `swprintf` into
the wide string at `s`. The conversions, flags, widths, precisions and
lengths are `printf`'s, and the numbers and `%p` are written by
`printf`'s own formatter, so they come out the same.

What differs:

| Conversion | Argument     | Written                                 |
|------------|--------------|-----------------------------------------|
| `%c`       | `int`        | the byte, as `btowc` converts it        |
| `%lc`      | `wint_t`     | the wide character                      |
| `%s`       | `char *`     | the UTF-8 string, as wide characters    |
| `%ls`      | `wchar_t *`  | the wide string                         |
| `%n`       | `int *`      | stores the wide characters so far       |

The width and the precision of `%s` and `%ls` count wide characters. A
`%s` string with bytes that are not UTF-8, within its precision, is an
encoding error: the call answers -1 and puts `EILSEQ` in `errno`. `%c`
of a value `btowc` refuses, anything outside 0 to 127, writes `WEOF`'s
character, 0xFFFF, as glibc writes its `WEOF`. A `NULL`
for `%s` or `%ls` writes `(null)`.

The answer is how many wide characters were written, not counting the
terminator, or -1 when a character could not be written to the stream,
did not fit, or was an encoding error.

`swprintf` writes at most `n - 1` characters and a terminator. Unlike
`snprintf`, it answers -1 when the whole did not fit, rather than the
length it would have had; what fit is in `s` all the same.

    wchar_t line[40];

    swprintf(line, 40, L"%ls: %d", name, score);

The wide functions pass a `double` to `printf`'s formatter themselves,
so a program that calls one carries the floating conversions whether
it prints a float or not.

## `vwprintf`, `vfwprintf`, `vswprintf`

    int vwprintf(const wchar_t *fmt, va_list ap);
    int vfwprintf(FILE *f, const wchar_t *fmt, va_list ap);
    int vswprintf(wchar_t *s, size_t n, const wchar_t *fmt, va_list ap);

`wprintf`, `fwprintf` and `swprintf` with the arguments in a `va_list`
the caller has started with `va_start`, as
[`vprintf`](04-stdio.md#vprintf-vfprintf-vsprintf-vsnprintf) is to
`printf`.

## `wscanf`, `fwscanf`, `swscanf`

    int wscanf(const wchar_t *fmt, ...);
    int fwscanf(FILE *f, const wchar_t *fmt, ...);
    int swscanf(const wchar_t *s, const wchar_t *fmt, ...);

[`scanf`](04-stdio.md#scanf-fscanf-sscanf) with a wide format, reading wide
characters: `wscanf` from `stdin`, `fwscanf` from `f` (each character
through `fgetwc`, so as UTF-8), and `swscanf` from the wide string `s`.
It is `scanf`'s reader: the conversions, widths, lengths, answers and
`errno` are `scanf`'s, white space is the same six ASCII characters,
and a width counts wide characters.

`%c`, `%s` and `%[` store as C99 says, by whether an `l` is written:

| Conversion            | Stores to    | What                             |
|-----------------------|--------------|----------------------------------|
| `%lc`, `%ls`, `%l[`   | `wchar_t *`  | the wide characters              |
| `%c`, `%s`, `%[`      | `char *`     | their UTF-8 bytes                |

A `char` buffer needs room for three bytes a character. An `h`, as in
`%hs`, stores bytes too.

A set, `%[...]`, takes any wide character its list names, those from 256
up included, and `^` turns it over for all of them.

    int n;
    wchar_t word[16];

    if (swscanf(L"17 coffee", L"%d %15ls", &n, word) == 2)
        ...

## `vwscanf`, `vfwscanf`, `vswscanf`

    int vwscanf(const wchar_t *fmt, va_list ap);
    int vfwscanf(FILE *f, const wchar_t *fmt, va_list ap);
    int vswscanf(const wchar_t *s, const wchar_t *fmt, va_list ap);

`wscanf`, `fwscanf` and `swscanf` with the arguments in a `va_list` the
caller has started with `va_start`.

## `wcstol`, `wcstoul`, `wcstoll`, `wcstoull`

    long wcstol(const wchar_t *s, wchar_t **end, int base);
    unsigned long wcstoul(const wchar_t *s, wchar_t **end, int base);
    long long wcstoll(const wchar_t *s, wchar_t **end, int base);
    unsigned long long wcstoull(const wchar_t *s, wchar_t **end,
                                int base);

[`strtol`](17-stdlib.md#strtol-strtoul-strtoll-strtoull) and its family
over a wide string, read in place by the same reader: the same white
space, signs, bases, limits and `ERANGE`, and `*end` left on the first
wide character not part of the number. Nothing past ASCII is a digit, a
sign or white space. The `intmax_t` ones,
[`wcstoimax` and `wcstoumax`](16-inttypes.md#wcstoimax-wcstoumax), are in
`inttypes.h`.

## `wcstod`, `wcstof`

    double wcstod(const wchar_t *s, wchar_t **end);
    float wcstof(const wchar_t *s, wchar_t **end);

[`strtod`](17-stdlib.md#strtod-strtof-atof) over a wide string, read by the
same reader: decimal and hex, `inf`, `infinity`, `nan`, correctly
rounded. `double` is `float` in acc, so the two are the same.

## `wcslen`

    size_t wcslen(const wchar_t *s);

The length of `s` in wide characters, not counting its terminator.

## `wcscpy`, `wcscat`

    wchar_t *wcscpy(wchar_t *to, const wchar_t *from);
    wchar_t *wcscat(wchar_t *to, const wchar_t *from);

[`strcpy` and `strcat`](06-string.md#strcpy-strcat-stpcpy) for wide
strings: `wcscpy` copies `from` and its terminator to `to`, and `wcscat`
to the end of the wide string at `to`. Both answer `to`, which must
have room; nothing checks.

## `wcsncpy`, `wcsncat`

    wchar_t *wcsncpy(wchar_t *to, const wchar_t *from, size_t n);
    wchar_t *wcsncat(wchar_t *to, const wchar_t *from, size_t n);

[`strncpy` and `strncat`](06-string.md#strncpy-strncat-stpncpy) for wide
strings. `wcsncpy` writes exactly `n` wide characters: `from`, then
zeros, and no terminator when `from` has `n` or more. `wcsncat` copies
at most `n` of `from` to the end of `to`, and then always a terminator.
Both answer `to`.

## `wcscmp`, `wcsncmp`, `wmemcmp`

    int wcscmp(const wchar_t *a, const wchar_t *b);
    int wcsncmp(const wchar_t *a, const wchar_t *b, size_t n);
    int wmemcmp(const wchar_t *a, const wchar_t *b, size_t n);

Compare two wide strings, `wcsncmp` at most their first `n` characters,
or `n` wide characters of memory for `wmemcmp`, which does not stop at
a 0. The answer is 0 when they are the same, -1 when `a` comes first,
and 1 when `b` does.

They compare the values of the `wchar_t`s, as C says, and a `wchar_t`
is signed: a character from 0x8000 to 0xFFFF sorts below every
character under 0x8000, and below the terminator too, so a string with
one sorts before the same string cut short in front of it.

## `wcscoll`, `wcsxfrm`

    int wcscoll(const wchar_t *a, const wchar_t *b);
    size_t wcsxfrm(wchar_t *to, const wchar_t *from, size_t n);

The C locale's collating order is `wcscmp`'s, so `wcscoll` is `wcscmp`.
`wcsxfrm` copies `from` and its terminator to `to` when `from` is
shorter than `n`, and copies nothing otherwise; either way it answers
`from`'s length.

## `wcschr`, `wcsrchr`

    wchar_t *wcschr(const wchar_t *s, wchar_t c);
    wchar_t *wcsrchr(const wchar_t *s, wchar_t c);

Where `c` first (`wcschr`) or last (`wcsrchr`) is in `s`, or `NULL` when
it is not there. The terminator counts as part of the string, so a `c`
of 0 finds the end.

## `wcsstr`

    wchar_t *wcsstr(const wchar_t *hay, const wchar_t *needle);

Where `needle` first is in `hay`, or `NULL` when it is not there. An
empty `needle` is found at `hay`.

## `wcsspn`, `wcscspn`, `wcspbrk`

    size_t wcsspn(const wchar_t *s, const wchar_t *accept);
    size_t wcscspn(const wchar_t *s, const wchar_t *reject);
    wchar_t *wcspbrk(const wchar_t *s, const wchar_t *accept);

[`strspn`, `strcspn` and
`strpbrk`](06-string.md#strspn-strcspn-strpbrk) for wide strings: how many
of `s`'s first characters are all in `accept`, how many are all not in
`reject`, and where the first one in `accept` is (or `NULL`).

## `wcstok`

    wchar_t *wcstok(wchar_t *s, const wchar_t *delim, wchar_t **save);

[`strtok`](06-string.md#strtok) for wide strings, but where it stopped is
kept in `*save` rather than a static variable, so two strings can be
split at once. The first call names the string; each call after it
passes `NULL` and the same `save`. It answers each token, with a
terminator written over the delimiter after it, and `NULL` at the end.

    wchar_t line[] = L"one, two", *save, *word;

    for (word = wcstok(line, L" ,", &save); word;
         word = wcstok(NULL, L" ,", &save))
        fputws(word, stdout);

## `wmemcpy`, `wmemmove`

    wchar_t *wmemcpy(wchar_t *to, const wchar_t *from, size_t n);
    wchar_t *wmemmove(wchar_t *to, const wchar_t *from, size_t n);

Copy `n` wide characters from `from` to `to`, and answer `to`. `wmemmove`
is right when the two overlap; `wmemcpy` copies forwards, so with an
overlap it is only right when `to` is below `from`. They are loops in
C, a character at a time, not the block instructions `memcpy` uses.

## `wmemset`, `wmemchr`

    wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n);
    wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n);

`wmemset` sets `n` wide characters from `s` to `c`, and answers `s`.
`wmemchr` answers the first of the `n` wide characters from `s` that is
`c`, or `NULL` when none is; a 0 does not stop it.

## `wcsftime`

    size_t wcsftime(wchar_t *s, size_t max, const wchar_t *fmt,
                    const struct tm *t);

[`strftime`](23-time.md#strftime), from `time.h`, with a wide format, writing wide characters:
the same writer, so every conversion is the same, in the C locale and
in UTC. A character of `fmt` that is not part of a conversion is copied
as it is.

The answer is how many wide characters were written, not counting the
terminator, or 0 when they and the terminator did not fit in `max`.
