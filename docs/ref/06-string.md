# `string.h`

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

## `memcpy`, `memmove`

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

## `memset`

    void *memset(void *s, int c, size_t n);

Fill `n` bytes from `s` with `c` (as an `unsigned char`), and answer `s`.

## `memcmp`

    int memcmp(const void *a, const void *b, size_t n);

Compare `n` bytes. The answer is 0 when they are all the same, and
otherwise the difference of the first two that differ: negative when
`a`'s is the smaller.

## `memchr`

    void *memchr(const void *s, int c, size_t n);

The first of the `n` bytes from `s` that is `c` (as an `unsigned char`),
or `NULL` when none is. It reads no further than the byte it finds.

## `strlen`, `strnlen`

    size_t strlen(const char *s);
    size_t strnlen(const char *s, size_t maxlen);

The length of `s`, not counting its terminator.

`strnlen` answers at most `maxlen`, and reads no further than
`s[maxlen - 1]`, so `s` need not have a terminator inside them. It is
POSIX's, not C99's.

## `strcpy`, `strcat`, `stpcpy`

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

## `strncpy`, `strncat`, `stpncpy`

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

## `strcmp`, `strncmp`

    int strcmp(const char *a, const char *b);
    int strncmp(const char *a, const char *b, size_t n);

Compare two strings, `strncmp` at most their first `n` characters. The
answer is 0 when they are the same, and otherwise the difference of the
first two characters that differ, as `unsigned char`: negative when `a`
comes first. A string that is the start of the other comes first.

## `strcasecmp`, `strncasecmp`

    int strcasecmp(const char *a, const char *b);
    int strncasecmp(const char *a, const char *b, size_t n);

`strcmp` and `strncmp` with the case of letters taken out: `'A'` to `'Z'`
are compared as `'a'` to `'z'`, as `tolower` has them in the C locale,
and no other character changes. Where the strings differ, the answer is
the difference of the two characters lowered. Both are POSIX's, not
C99's, and acc declares them in `string.h` (POSIX puts them in
`strings.h`, which acc does not have).

## `strchr`, `strrchr`, `strchrnul`

    char *strchr(const char *s, int c);
    char *strrchr(const char *s, int c);
    char *strchrnul(const char *s, int c);

`strchr` answers where `c` (as a `char`) first is in `s`, and `strrchr`
where it last is; both answer `NULL` when it is not there. The
terminator counts as part of the string, so a `c` of 0 finds the end.

`strchrnul` is `strchr` answering the end of `s`, its terminator, rather
than `NULL` when `c` is not there. It is GNU's, not C99's.

## `strstr`

    char *strstr(const char *hay, const char *needle);

Where `needle` first is in `hay`, or `NULL` when it is not there. An
empty `needle` is found at `hay`.

## `strspn`, `strcspn`, `strpbrk`

    size_t strspn(const char *s, const char *accept);
    size_t strcspn(const char *s, const char *reject);
    char *strpbrk(const char *s, const char *accept);

`strspn` counts how many of `s`'s first characters are all in `accept`,
and `strcspn` how many are all not in `reject`. `strpbrk` answers where
the first character of `s` that is in `accept` is, or `NULL` when none
is.

    s += strspn(s, " \t");          /* past the blanks */
    word = strcspn(s, " \t");       /* the length of the next word */

## `strtok`

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

## `strcoll`, `strxfrm`

    int strcoll(const char *a, const char *b);
    size_t strxfrm(char *to, const char *from, size_t n);

The locale's collating order. acc has only the C locale, where the order
is the bytes' own, so `strcoll` is `strcmp`.

`strxfrm` copies `from` and its terminator to `to` when it is shorter
than `n`, and copies nothing otherwise; either way it answers `from`'s
length. A call with an `n` of 0 (and `to` `NULL`) measures the room
needed, less one.

## `strdup`, `strndup`

    char *strdup(const char *s);
    char *strndup(const char *s, size_t n);

A copy of `s` in room from `malloc`, to be given back with `free`.
`strndup` copies at most `n` characters of `s` (which need not have a
terminator inside them) and always adds one. Both answer `NULL`, and
copy nothing, when `malloc` has no room. They are POSIX's, not C99's.

## `strerror`

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
