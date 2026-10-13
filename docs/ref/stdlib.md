# `stdlib.h`

The heap, sorting and searching, random numbers, numbers read out of
strings, multibyte characters, and the end of the program: C99's
`stdlib.h`, less `strtold`, and with Microsoft's `itoa`, `ltoa` and
`ultoa` and OpenBSD's `strtonum` added.

The header also gives `size_t` (an unsigned int, 24 bits), `wchar_t` (a
`short`, 16 bits), `NULL`, and no other name of `stddef.h`; the
structures `div_t`, `ldiv_t` and `lldiv_t`; and the macros `RAND_MAX`
(32767), `EXIT_SUCCESS` (0), `EXIT_FAILURE` (1) and `MB_CUR_MAX` (3).

`float` and `double` are the same four-byte type on acc, and acc
refuses `long double`, so there is no `strtold`, and `strtod` reads to a
float's precision.

## `malloc`, `calloc`, `realloc`, `free`

    void *malloc(size_t n);
    void *calloc(size_t count, size_t size);
    void *realloc(void *p, size_t n);
    void  free(void *p);

`malloc` answers room for `n` bytes, or `NULL` when there is no free
block that large. `malloc(0)` answers `NULL`. The bytes are not cleared.

`calloc` is `malloc` of `count * size` bytes, all set to zero. A product
past what a `size_t` holds (16 MB) answers `NULL`.

`free` gives a block back; `free(NULL)` does nothing. A freed block is
joined to whichever of its neighbours is free, so the room a program
gives back is room it can ask for again as one piece. Nothing checks
that the pointer came from `malloc`, or that it was not freed already.

`realloc` changes a block's size and answers where it now is:

- `realloc(NULL, n)` is `malloc(n)`.
- `realloc(p, 0)` frees `p` and answers `NULL`.
- `n` no larger than the block already is: the same block, unchanged. A
  block is never made smaller, and nothing is given back.
- Larger, and the block after it is free and large enough: the block
  grows into it, in place, and nothing is copied.
- Otherwise a new block from `malloc`, the old one's bytes copied to it,
  and the old one freed. When there is no room, the answer is `NULL` and
  the old block is left as it was, still the program's.

The heap is the memory between the end of the program's variables and
16 KB below the top of the program's memory, the 16 KB being the stack's.
A program has 448 KB for its code, its variables, the heap and the stack,
so the heap is 432 KB less the program; a moslet has 32 KB in all, and
so less than 16 KB of heap. Nothing stops a stack that goes deeper than
16 KB from running into the heap.

Each block costs 10 bytes beside its own, and a request is rounded up a
little and is never less than 6 bytes. `malloc` takes the first free
block large enough, walking only the free blocks, and splits off what is
left over as a free block of its own when that is large enough to be
worth having.

## `qsort`

    void qsort(void *base, size_t nmemb, size_t size,
               int (*cmp)(const void *, const void *));

Sort the `nmemb` elements of `size` bytes at `base` into the order `cmp`
gives. `cmp` is handed pointers to two elements and answers a negative
number when the first goes first, 0 when they are equal, and a positive
number when the second goes first.

    static int by_number(const void *a, const void *b)
    {
        int x = *(const int *) a, y = *(const int *) b;

        return (x > y) - (x < y);
    }

    qsort(numbers, count, sizeof numbers[0], by_number);

It is a shell sort, not a quicksort: it uses no stack beyond its own
frame, and with Knuth's gaps (1, 4, 13, 40, ...) it is O(n^1.5) and has
no bad case to fear. It is **not stable**: equal elements may end up in
any order. Elements are exchanged a byte at a time. Fewer than two
elements, or a `size` of 0, are left alone.

## `bsearch`

    void *bsearch(const void *key, const void *base, size_t nmemb,
                  size_t size, int (*cmp)(const void *, const void *));

Find `key` in the `nmemb` elements of `size` bytes at `base`, which are
sorted in `cmp`'s order, by halving. `cmp` is handed `key` first and an
element second, and answers as `qsort`'s does. The answer is an element
equal to `key`, or `NULL` when none is. Where several are equal, it is
any one of them, not necessarily the first.

## `rand`, `srand`, `RAND_MAX`

    #define RAND_MAX 32767

    int  rand(void);
    void srand(unsigned seed);

`rand` answers the next number of a sequence, from 0 to `RAND_MAX`.
`srand` starts the sequence again from `seed`; a program that never calls
it gets the sequence `srand(1)` gives, the same each run.

The generator is the one C99 gives as its example: a 32-bit state, times
1103515245, plus 12345, a number; `rand` answers bits 16 to 30 of it.
`seed` is an `unsigned`, so only 24 bits of it.

## `abs`, `labs`, `llabs`

    int       abs(int n);
    long      labs(long n);
    long long llabs(long long n);

The magnitude of `n`.

## `div`, `ldiv`, `lldiv`, `div_t`, `ldiv_t`, `lldiv_t`

    typedef struct { int quot, rem; } div_t;
    typedef struct { long quot, rem; } ldiv_t;
    typedef struct { long long quot, rem; } lldiv_t;

    div_t   div(int a, int b);
    ldiv_t  ldiv(long a, long b);
    lldiv_t lldiv(long long a, long long b);

`a / b` in `quot` and `a % b` in `rem`, both at once. The quotient is
truncated toward zero, as C99's `/` is, so `rem` has `a`'s sign. They are
the two operators and nothing more, so `b` must not be 0.

## `atoi`, `atol`, `atoll`

    int       atoi(const char *s);
    long      atol(const char *s);
    long long atoll(const char *s);

The base 10 number at the front of `s`: spaces (and `\t`, `\n`, `\v`,
`\f`, `\r`) skipped, a `+` or `-`, then digits. Reading stops at the
first character that is not a digit, and nothing says where that was or
whether there were any digits at all: a string with none answers 0.

There is no check for range. A number too large for the type wraps, and
`errno` is not set. `atoi` is `atol` cut to an `int`, which is 24 bits.
`strtol` and `strtonum` are the ones that say what went wrong.

## `strtol`, `strtoul`, `strtoll`, `strtoull`

    long               strtol(const char *s, char **end, int base);
    unsigned long      strtoul(const char *s, char **end, int base);
    long long          strtoll(const char *s, char **end, int base);
    unsigned long long strtoull(const char *s, char **end, int base);

The integer at the front of `s`, in `base`. Spaces (and `\t`, `\n`, `\v`,
`\f`, `\r`) are skipped, then a `+` or `-`, then the digits: `0` to `9`
and then `a` to `z` (or `A` to `Z`) for 10 to 35, as many of them as are
below `base`.

`base` is 2 to 36, or 0. Base 16 allows `0x` or `0X` in front of the
digits. Base 0 takes the base from how the number is written: `0x` in
front is 16, a `0` in front is 8, and anything else is 10. Any other base
answers 0 with `*end` at `s`, and leaves `errno` alone.

When `end` is not `NULL`, `*end` is left on the first character that is
not part of the number, and on `s` itself when there were no digits. A
`0x` with no hex digit after it is the number 0 followed by an `x`.

    const char *text = "  42mb";
    char *end;
    long n = strtol(text, &end, 10);    /* 42, end at "mb" */

    if (end == text)
        puts("no number");

A number past the type's range is read to its end all the same, and the
answer is the type's limit (`LONG_MIN` or `LONG_MAX`, `ULONG_MAX`, and so
on) with `ERANGE` in `errno`. `errno` is not otherwise changed, so a
program that wants to see `ERANGE` sets it to 0 first.

A `-` in front of a number for `strtoul` or `strtoull` negates it in the
unsigned type, as C says: `strtoul("-1", NULL, 10)` is `ULONG_MAX`. A
magnitude past the limit is the limit, minus sign or not.

## `strtonum`

    long long strtonum(const char *nptr, long long minval,
                       long long maxval, const char **errstr);

The base 10 number that is the whole of `nptr`, checked to lie between
`minval` and `maxval`. Spaces in front and a sign are allowed; anything
after the digits is not. It is OpenBSD's, not C99's.

On success the answer is the number, `*errstr` is `NULL`, and `errno` is
as it was. Otherwise the answer is 0, and `*errstr` says why:

| `*errstr`     | When                                     | `errno`  |
|---------------|------------------------------------------|----------|
| `"invalid"`   | no digits, or something after them       | `EINVAL` |
| `"invalid"`   | `minval` is greater than `maxval`        | `EINVAL` |
| `"too small"` | below `minval`                           | `ERANGE` |
| `"too large"` | above `maxval`                           | `ERANGE` |

`errstr` may be `NULL` when only the answer matters.

    const char *why;
    long long port = strtonum(argv[1], 1, 65535, &why);

    if (why) {
        printf("port is %s: %s\n", why, argv[1]);
        return 1;
    }

## `strtod`, `strtof`, `atof`

    double strtod(const char *s, char **end);
    float  strtof(const char *s, char **end);
    double atof(const char *s);

The floating number at the front of `s`. Spaces (and `\t`, `\n`, `\v`,
`\f`, `\r`) are skipped, then a `+` or `-`, then any of:

- decimal digits with a point among them or not, and then an exponent:
  `e` or `E`, a sign, and digits;
- `0x` or `0X`, hex digits with a point among them or not, and then a
  binary exponent: `p` or `P`, a sign, and decimal digits;
- `INF` or `INFINITY`, or `NAN`, in either case. `NAN` may be followed
  by letters, digits and `_` in parentheses, which are passed over.

An exponent that is not complete (an `e` with no digit after it) is not
part of the number. When `end` is not `NULL`, `*end` is left on the first
character that is not part of the number, and on `s` when there was no
number, which answers 0. As for `strtol`, a `0x` with no hex digit after
it is the number 0 followed by an `x`.

The answer is correctly rounded: the float nearest the number written,
ties to even, however many digits it has. `double` is the same four-byte
type as `float`, so `strtod` and `strtof` answer the same.

A number too large for a float answers `HUGE_VAL`, which is infinity,
with its sign, and puts `ERANGE` in `errno`. One too small to be held
exactly answers a denormal or 0 and puts `ERANGE` in `errno`, as glibc
does. `errno` is not otherwise changed.

`atof(s)` is `strtod(s, NULL)`.

## `itoa`, `ltoa`, `ultoa`

    char *itoa(int value, char *str, int base);
    char *ltoa(long value, char *str, int base);
    char *ultoa(unsigned long value, char *str, int base);

Write `value`'s digits in `base` to `str`, with a terminator, and answer
`str`. `base` is 2 to 36, and the digits past 9 are lowercase letters; a
`base` outside that writes the empty string.

Only base 10 has a sign: a negative value is written with a `-`. In any
other base a negative value is its two's complement bits read unsigned,
and an `int` has 24 of them:

    itoa(-1, s, 16);        /* "ffffff" */
    ltoa(-1, s, 16);        /* "ffffffff" */
    itoa(-42, s, 10);       /* "-42" */

Nothing checks the room at `str`. The most it takes is an `int` in base
2, 25 bytes with the terminator, and a `long` or `unsigned long` in base
2, 33.

They are Microsoft's and DOS's, not C99's, and agondev does not have
them. Each is in the library on its own, so a program may define its own
function of one of these names.

## `mblen`, `mbtowc`, `wctomb`, `MB_CUR_MAX`

    #define MB_CUR_MAX 3

    int mblen(const char *s, size_t n);
    int mbtowc(wchar_t *pwc, const char *s, size_t n);
    int wctomb(char *s, wchar_t wc);

Multibyte characters are UTF-8, as far as a 16-bit `wchar_t` goes: one
byte below 0x80, two below 0x800, and three for the rest up to 0xFFFF,
so `MB_CUR_MAX` is 3. A character UTF-8 writes in four bytes cannot be
held in a `wchar_t`, and is refused.

`mbtowc` reads the character at `s`, looking at no more than `n` bytes,
puts it in `*pwc` (when `pwc` is not `NULL`), and answers how many bytes
it took; at a terminator it answers 0. `mblen` is `mbtowc` without
`pwc`.

`wctomb` writes `wc`'s bytes to `s`, which needs room for `MB_CUR_MAX`,
and answers how many it wrote. A `wchar_t` is a `short`, so values below
0 are the characters 0x8000 to 0xFFFF.

All three answer -1 for what is not a character: a byte that cannot
start one or cannot continue one, a character written with more bytes
than it needs, or one of the surrogates 0xD800 to 0xDFFF. Then `errno` is
`EILSEQ`. A character cut short by `n` also answers -1; `mbtowc` puts
`EILSEQ` in `errno` for it, and `mblen` leaves `errno` alone.

UTF-8 has no shift states, so each answers 0 when `s` is `NULL`. The
restartable `mbrtowc`, `mbrlen` and `wcrtomb` are in `wchar.h`.

## `mbstowcs`, `wcstombs`

    size_t mbstowcs(wchar_t *dst, const char *src, size_t n);
    size_t wcstombs(char *dst, const wchar_t *src, size_t n);

`mbstowcs` converts the UTF-8 string `src` to wide characters at `dst`,
at most `n` of them, and answers how many it wrote, not counting the
terminator. The terminator is written only when there is room for it
within the `n`.

`wcstombs` converts the wide string `src` to UTF-8 at `dst`, at most `n`
bytes, and never part of a character: one that does not fit whole is
not written. It answers how many bytes it wrote, not counting the
terminator, which again is written only when there is room.

With `dst` `NULL`, both write nothing, ignore `n`, and answer how many
there would be: the room to allocate, less one for the terminator.

Either answers `(size_t) -1`, with `EILSEQ` in `errno`, at something
that is not a character, as `mbtowc` and `wctomb` judge it.

## `exit`, `_Exit`, `EXIT_SUCCESS`, `EXIT_FAILURE`

    #define EXIT_SUCCESS 0
    #define EXIT_FAILURE 1

    void exit(int status);
    void _Exit(int status);

`exit` ends the program as returning `status` from `main` would. The
functions `atexit` registered run, the last registered first, and then
every file still open is flushed and closed; `status` then goes back to
MOS. A call to `exit` by name is compiled in place, as a jump back to
where `main` was called, and costs no library code; a pointer to `exit`
reaches a library function that does the same.

`_Exit` ends the program with `status` without running the `atexit`
functions and without closing files, so what is still in a file's buffer
is not written.

Neither returns. What happens to `status` is the program's choice of
`acc` flag: by default it goes back to MOS in HL; `-p` prints it as six
hex digits, and `-x` writes it to IO port 0.

## `atexit`

    int atexit(void (*f)(void));

Register `f` to be called when the program ends, by returning from
`main` or by `exit`. The functions run the last registered first, before
the files are closed. `_Exit` and `abort` run none of them.

The answer is 0, or -1 when 32 functions are registered already; 32 is
the most there may be. The same function may be registered more than
once, and then runs as many times.

## `abort`

    void abort(void);

End the program as having gone wrong. It raises `SIGABRT` first when the
program has called `signal`: a handler for it runs, and if that handler
returns, or the signal is ignored, the program ends anyway.

It prints nothing. The program ends through `_Exit` with the status 134
(128 and `SIGABRT`'s 6, as a shell reports a program `SIGABRT` ended), so
the `atexit` functions do not run and files are not closed.

## `getenv`

    char *getenv(const char *name);

MOS has no environment, so `getenv` always answers `NULL`.

## `system`

    int system(const char *command);

Run `command` on MOS's command line, as though typed at its prompt.
`system(NULL)` answers 1: there is a command processor. Otherwise the
answer is what MOS answers: 0 when the command was done, or MOS's error
number.

A command that runs a program loads that program over the one that
called `system`, which then does not come back, unless the program run
is a moslet.
