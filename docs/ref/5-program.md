# The program

Memory, numbers from strings, sorting, the end of the program, errors, signals, jumps, variable arguments and the time.

## `stdlib.h`

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

### `malloc`, `calloc`, `realloc`, `free`

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

### `qsort`

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

### `bsearch`

    void *bsearch(const void *key, const void *base, size_t nmemb,
                  size_t size, int (*cmp)(const void *, const void *));

Find `key` in the `nmemb` elements of `size` bytes at `base`, which are
sorted in `cmp`'s order, by halving. `cmp` is handed `key` first and an
element second, and answers as `qsort`'s does. The answer is an element
equal to `key`, or `NULL` when none is. Where several are equal, it is
any one of them, not necessarily the first.

### `rand`, `srand`, `RAND_MAX`

    #define RAND_MAX 32767

    int  rand(void);
    void srand(unsigned seed);

`rand` answers the next number of a sequence, from 0 to `RAND_MAX`.
`srand` starts the sequence again from `seed`; a program that never calls
it gets the sequence `srand(1)` gives, the same each run.

The generator is the one C99 gives as its example: a 32-bit state, times
1103515245, plus 12345, a number; `rand` answers bits 16 to 30 of it.
`seed` is an `unsigned`, so only 24 bits of it.

### `abs`, `labs`, `llabs`

    int       abs(int n);
    long      labs(long n);
    long long llabs(long long n);

The magnitude of `n`.

### `div`, `ldiv`, `lldiv`, `div_t`, `ldiv_t`, `lldiv_t`

    typedef struct { int quot, rem; } div_t;
    typedef struct { long quot, rem; } ldiv_t;
    typedef struct { long long quot, rem; } lldiv_t;

    div_t   div(int a, int b);
    ldiv_t  ldiv(long a, long b);
    lldiv_t lldiv(long long a, long long b);

`a / b` in `quot` and `a % b` in `rem`, both at once. The quotient is
truncated toward zero, as C99's `/` is, so `rem` has `a`'s sign. They are
the two operators and nothing more, so `b` must not be 0.

### `atoi`, `atol`, `atoll`

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

### `strtol`, `strtoul`, `strtoll`, `strtoull`

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

### `strtonum`

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

### `strtod`, `strtof`, `atof`

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

### `itoa`, `ltoa`, `ultoa`

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

### `mblen`, `mbtowc`, `wctomb`, `MB_CUR_MAX`

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

### `mbstowcs`, `wcstombs`

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

### `exit`, `_Exit`, `EXIT_SUCCESS`, `EXIT_FAILURE`

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

### `atexit`

    int atexit(void (*f)(void));

Register `f` to be called when the program ends, by returning from
`main` or by `exit`. The functions run the last registered first, before
the files are closed. `_Exit` and `abort` run none of them.

The answer is 0, or -1 when 32 functions are registered already; 32 is
the most there may be. The same function may be registered more than
once, and then runs as many times.

### `abort`

    void abort(void);

End the program as having gone wrong. It raises `SIGABRT` first when the
program has called `signal`: a handler for it runs, and if that handler
returns, or the signal is ignored, the program ends anyway.

It prints nothing. The program ends through `_Exit` with the status 134
(128 and `SIGABRT`'s 6, as a shell reports a program `SIGABRT` ended), so
the `atexit` functions do not run and files are not closed.

### `getenv`

    char *getenv(const char *name);

MOS has no environment, so `getenv` always answers `NULL`.

### `system`

    int system(const char *command);

Run `command` on MOS's command line, as though typed at its prompt.
`system(NULL)` answers 1: there is a command processor. Otherwise the
answer is what MOS answers: 0 when the command was done, or MOS's error
number.

A command that runs a program loads that program over the one that
called `system`, which then does not come back, unless the program run
is a moslet.

## `errno.h`

The number a library function leaves when it fails, and the names of
the numbers it leaves.

The header gives `errno` and four error numbers: C's `EDOM`, `ERANGE`
and `EILSEQ`, and POSIX's `EINVAL`. The numbers are Linux's, so a
number printed on the Agon means what it would mean anywhere else.
Nothing in the library sets `errno` to any other.

### `errno`

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
[`stdio.h`](2-io.md#stdioh)), nor do `time.h`'s or `signal`. The words for a
number are [`strerror`](3-strings.md#strerror)'s, and
[`perror`](2-io.md#perror) prints those for `errno`.

### `EDOM`

    #define EDOM 33

An argument outside the function's domain. The functions of `math.h`
set it, and answer a NaN, as C asks when
[`math_errhandling`](4-numbers.md#math_errhandling-math_errno-math_errexcept)
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

### `ERANGE`

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

- [`strtol`](#strtol-strtoul-strtoll-strtoull), `strtoul`,
  `strtoll`, `strtoull`, `strtoimax` and `strtoumax`, and the `wcsto`
  forms of each, for a number past the type's range;
- [`strtod`](#strtod-strtof-atof), `strtof`, `atof`, `wcstod`
  and `wcstof`, for a number too large for a float, or too small to be
  held exactly;
- [`strtonum`](#strtonum), for a number outside the range it is
  given;
- the [`scanf`](2-io.md#scanf-fscanf-sscanf) family, for a number it
  reads that is out of range.

### `EILSEQ`

    #define EILSEQ 84

Bytes that are not a character: not UTF-8, or a character outside the
16 bits of a `wchar_t`, or a wide character that is one of the
surrogates. The multibyte and wide character conversions set it: of
`stdlib.h`, [`mblen`](#mblen-mbtowc-wctomb-mb_cur_max),
`mbtowc`, `wctomb`, [`mbstowcs`](#mbstowcs-wcstombs) and
`wcstombs`; of `wchar.h`, `mbrtowc`, `mbrlen`, `wcrtomb`, `mbsrtowcs`
and `wcsrtombs`; and `fgetwc` and `fputwc`, and so the wide reads and
writes made of them.

### `EINVAL`

    #define EINVAL 22

An argument that is not valid. [`strtonum`](#strtonum) sets
it for a string that is not a number, and for a `minval` greater than
`maxval`; [`signal`](#signal-sig_dfl-sig_ign-sig_err) and
[`raise`](#raise) for a signal there is no such thing as. It is
POSIX's, not C99's.

## `assert.h`

The `assert` macro, which stops a program when something it takes for
granted is not so, and `NDEBUG`, which turns it off.

### `assert`

    void assert(scalar e);

When `e` is 0, print what failed to `stderr` and end the program;
otherwise do nothing. `e` is evaluated once.

The message names the file, the line, the function and the expression
as it was written, in the words glibc uses:

    main.c:12: parse: Assertion `n < MAX' failed.

`assert` then ends the program through `abort`, so its status is 134,
the `atexit` functions do not run, and files are not closed. A program
that set a handler for `SIGABRT` with `signal` has it called first.

The message and the call are a function of the library's own, which
brings `fprintf` with it into the program. `stderr` is not buffered, so
the message is on the screen before the program ends.

### `NDEBUG`

    #define NDEBUG

When `NDEBUG` is defined where `assert.h` is included, `assert(e)` is
`((void) 0)`: `e` is not evaluated, and the program carries neither the
message nor `fprintf`. Define it in front of the `#include`, or with
`-D NDEBUG` on acc's command line.

The header has no include guard round the macro: each time it is
included, `assert` is defined again by whether `NDEBUG` is defined
then, so one file can have it on in one part and off in another.

## `signal.h`

Signals, which on the Agon are only the ones a program raises itself.

The header gives `sig_atomic_t` (an `int`, 24 bits), the handlers
`SIG_DFL`, `SIG_IGN` and `SIG_ERR`, and the six signals C names, with
the numbers Linux gives them.

Nothing on the Agon sends a program a signal. Escape at the keyboard is
the end of `stdin`'s input, not `SIGINT`; a division by zero or a
floating point error raises no `SIGFPE`; and nothing raises `SIGSEGV`
or `SIGILL`. A signal reaches a handler only when the program calls
`raise`, or `abort`, which raises `SIGABRT`.

### `SIGABRT`, `SIGFPE`, `SIGILL`, `SIGINT`, `SIGSEGV`, `SIGTERM`

    #define SIGINT  2
    #define SIGILL  4
    #define SIGABRT 6
    #define SIGFPE  8
    #define SIGSEGV 11
    #define SIGTERM 15

The signals C names. Any number from 1 to 15 is a signal to `signal` and
`raise`, these six and the nine between them that have no name.

### `signal`, `SIG_DFL`, `SIG_IGN`, `SIG_ERR`

    #define SIG_DFL ((void (*)(int)) 0)
    #define SIG_ERR ((void (*)(int)) -1)
    #define SIG_IGN ((void (*)(int)) 1)

    void (*signal(int sig, void (*func)(int)))(int);

Set what happens when `sig` is raised, and answer what was set before:

- `SIG_DFL`, which every signal starts with, ends the program.
- `SIG_IGN` ignores the signal.
- A function is called with the signal's number, as its handler.

A `sig` that is not 1 to 15 answers `SIG_ERR`, changes nothing, and
puts `EINVAL` in `errno`.

### `raise`

    int raise(int sig);

Raise `sig` in the program, now, and answer 0 once it is dealt with;
a `sig` that is not 1 to 15 answers -1 and puts `EINVAL` in `errno`.

A handler is called with `sig` after the signal has been put back to
`SIG_DFL`, so a handler that raises its own signal ends the program
rather than calling itself for ever. To catch the signal again, the
handler calls `signal` for it once more. When the handler returns,
`raise` returns 0.

With `SIG_DFL`, the program ends with the status 128 and `sig` (134 for
`SIGABRT`), which is what a shell reports for a program a signal ended.
It ends through `_Exit`, so the `atexit` functions do not run and files
are not closed.

    static volatile sig_atomic_t stop;

    static void on_term(int sig)
    {
        stop = 1;
    }

    signal(SIGTERM, on_term);
    ...
    raise(SIGTERM);                 /* on_term runs; stop is 1 */

`abort` raises `SIGABRT` when the program has called `signal`; see
[`abort`](#abort).

### `sig_atomic_t`

    typedef int sig_atomic_t;

The type of a variable a handler sets for the rest of the program to
see, as `stop` above: an `int`, 24 bits. Declare it `volatile`, so
that each read in a loop reads it again.

## `setjmp.h`

Jumps from a function back to one that called it, past the returns in
between.

The header gives `jmp_buf`. `setjmp` and `longjmp` are in acc's runtime,
written in eZ80 assembly, since they put back the stack pointer and the
frame pointer, which C cannot name.

### `setjmp`, `longjmp`, `jmp_buf`

    typedef int jmp_buf[4];

    int  setjmp(jmp_buf env);
    void longjmp(jmp_buf env, int val);

`setjmp` saves in `env` the place it was called from and answers 0.
`longjmp` goes back there: `setjmp` returns a second time, answering
`val`, or 1 when `val` is 0, so that the second return is never taken
for the first. `longjmp` does not return.

    static jmp_buf on_error;

    if (setjmp(on_error)) {
        puts("bad input");
        return 1;
    }
    parse(text);            /* calls longjmp(on_error, 1) to give up */

A `jmp_buf` is four words, 12 bytes: the return address of the call to
`setjmp`, IX, the stack pointer as it was at the call, and IY. That is
all a call keeps across it in registers. The function's other locals
live in its frame, where IX points, so after `longjmp` they hold
whatever values they had when `longjmp` was called. IY is the one
local acc can keep in a register; it is saved and put back, so a
local held there is as it was when `setjmp` was called. C says the
value of a local changed between the two is uncertain unless it is
`volatile`, and a program that has to be right on other compilers too
declares those locals `volatile`.

acc knows `setjmp` by its name, and compiles a call to it with no value
kept in registers around it: call it by its name, not through a pointer.
The function that called `setjmp` must still be running when `longjmp`
is called, since `longjmp` goes back into its frame.

## `stdarg.h`

Reading the arguments a function declared with `...` was given past its
named parameters.

The header gives the type `va_list` and the four macros C99 names. They
are spelled over acc's own words for them, `__va_list`, `__va_start`,
`__va_arg`, `__va_end` and `__va_copy`, which are part of the language
rather than macros written in C, since `va_arg` has to know how wide a
value is to step over it. A program that does not include the header
may use `va_list` and the rest as names of its own, and one that does
can ask `#ifdef va_copy`.

### `va_list`, `va_start`, `va_arg`, `va_end`

    typedef __va_list va_list;

    void va_start(va_list ap, last);
    type va_arg(va_list ap, type);
    void va_end(va_list ap);

A `va_list` is a `char *` that walks along the arguments on the stack.
`va_start` points it just past `last`, the function's last named
parameter. Each `va_arg` then reads a value of `type` where it points,
and steps it past the room that value took; `va_end` has nothing to
undo, and does nothing.

    int sum(int count, ...)
    {
        va_list ap;
        int total = 0;

        va_start(ap, count);
        while (count-- > 0)
            total += va_arg(ap, int);
        va_end(ap);

        return total;
    }

Every argument fills whole three-byte slots, as the call pushed it; a
`struct` is rounded up to whole slots:

| Argument                           | Room              |
|------------------------------------|-------------------|
| `char`, `short`, `int`, a pointer  | one slot, 3 bytes |
| `long`, `float`, `double`          | two, 6 bytes      |
| `long long`                        | three, 9 bytes    |
| a `struct` or `union`              | its size, rounded |

A `char` or a `short` argument is passed as an `int`, so read it with
`va_arg(ap, int)`. `float` and `double` are the same four-byte type on
acc, so `va_arg(ap, double)` reads a `float` argument as well.

`va_arg` reads what it is told to: nothing checks the type against the
argument, or that there is an argument left to read. The function must
know from its named parameters how many there are and what they are, as
`printf` knows from its format.

acc refuses `va_start` in a function that has no `...`, and a `last`
that is not one of the function's parameters. It refuses a `va_arg` of
a function type, and any `ap` that is not a `va_list`. `ap` may be any
expression that names one, such as `*p` for a `va_list *p` or an
element of an array of them, and the value of `va_arg` may be
subscripted or called like any other.

### `va_copy`

    void va_copy(va_list to, va_list from);

Make `to` a copy of `from`, at the same place among the arguments, so
that the arguments can be read twice. It is an assignment, `to = from`.

    va_list again;

    va_copy(again, ap);
    n = vsnprintf(NULL, 0, fmt, again);     /* measure */
    va_end(again);
    vsprintf(buf, fmt, ap);                 /* then write */

A `va_list` handed to a function, such as
[`vprintf`](2-io.md#vprintf-vfprintf-vsprintf-vsnprintf), is a copy
of the pointer: what the function reads with it does not move the
caller's.

## `time.h`

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

### `time_t`, `tm`

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

### `time`

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

### `clock`, `clock_t`, `CLOCKS_PER_SEC`

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

### `difftime`

    double difftime(time_t t1, time_t t0);

`t1 - t0`, in seconds, as a `double`. A `double` is the same four-byte
type as a `float` on acc, so the answer is exact only up to 16777216
seconds (about 194 days); subtract the two `time_t`s, which are
integers, for a longer span to the second.

### `mktime`

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

`mktime` writes only `*t`: the `struct tm` that `gmtime` and
`localtime` last answered is left as it was.

### `gmtime`, `localtime`

    struct tm *gmtime(const time_t *t);
    struct tm *localtime(const time_t *t);

`*t` as a date and a time of day, UTC, with every field filled in and
`tm_isdst` 0. `localtime` is `gmtime`, since the Agon keeps no time zone.

Both answer the same static `struct tm`, which the next call of either,
of `mktime` or of `ctime`, writes over. They never answer `NULL`; a
time whose year does not fit in an `int` loses its top bits.

### `asctime`, `ctime`

    char *asctime(const struct tm *t);
    char *ctime(const time_t *t);

The time as C's fixed form for it, with a `'\n'` at the end:

    Thu Jan  1 00:00:00 1970

The day of the week and the month as three letters, the day of the
month in two places (a space in front of one digit), the time, and the
year. It is `strftime`'s `"%a %b %e %H:%M:%S %Y\n"`.

`ctime(t)` is `asctime(localtime(t))`. Both answer the same static
buffer, which the next call of either writes over.

### `strftime`

    size_t strftime(char *s, size_t max, const char *fmt,
                    const struct tm *t);

Write the time in `*t` into `s` as `fmt` says, as `printf` writes
numbers: characters other than `%` are copied, and a `%` and the
character after it are a conversion. The locale is C's, the only one
acc has. [`wcsftime`](2-io.md#wcsftime), in `wchar.h`, is the same
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
tm` that `gmtime` or `mktime` filled in. A `tm_wday` or `tm_mon` out of
range has the name `?`, as glibc writes it.

The answer is the number of characters written, not counting the
terminator that follows them. When they and the
terminator do not fit in `max`, the answer is 0, and `s` holds the
first `max` characters with no terminator.

    char line[32];

    strftime(line, sizeof line, "%Y-%m-%d %H:%M", gmtime(&now));
