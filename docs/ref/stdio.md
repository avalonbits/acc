# `stdio.h`

Streams: the keyboard and the screen, files on the SD card through MOS,
and the formatted reading and writing of `printf` and `scanf` into
either of them or into a string.

The header gives `FILE`, `fpos_t` (a `long`, the place in a file),
`size_t` (an unsigned int, 24 bits), `NULL`, `EOF`, `BUFSIZ`,
`FOPEN_MAX`, `FILENAME_MAX`, `L_tmpnam`, `TMP_MAX`, the `SEEK_` and
`_IO` constants, and the three standard streams `stdin`, `stdout` and
`stderr`. What is inside a `FILE` is the library's business.

The file functions do not set `errno`. One that fails says so by its
answer (and the stream's error flag), and `errno` keeps whatever it
held before, so `perror` after a failed `fopen` does not describe it.
Only the `scanf` family sets it, to `ERANGE`, for a number out of
range.

A program that only prints carries none of the file layer: `printf`,
`putchar` and `puts` write to the screen themselves, and the file
functions, their buffers and their table come into a program only when
it calls one of them.

## `stdin`, `stdout`, `stderr`

    extern FILE *stdin;
    extern FILE *stdout;
    extern FILE *stderr;

`stdin` is the keyboard, read a line at a time through MOS's line
editor. Nothing reaches the program until Enter is pressed; then the
library writes a return and a line feed to the screen, and the program
reads the line with a `'\n'` after it. The line editor is given room for
255 bytes. Escape is the end of the input: the library writes a return
and a line feed, and `stdin` answers `EOF` from then on, until
`clearerr` (or `ungetc`) takes the end-of-file flag off it.

`stdout` and `stderr` are the screen, and are not buffered: every
character goes to MOS as it is written, so there is nothing to flush.
Every `'\n'` written to the screen, by any of the functions here, is a
return and a line feed, which is what MOS's console wants and what
agondev's library sends. `putch` (`<agon/mos.h>`) is the way to send a
byte as it is, such as a number in a VDU command.

`freopen` turns any of the three into a file. Closing one that is still
the keyboard or the screen does nothing, and answers 0.

## `EOF`, `BUFSIZ`, `FOPEN_MAX`, `FILENAME_MAX`, `L_tmpnam`, `TMP_MAX`

| Name           | Value | Meaning                                   |
|----------------|-------|-------------------------------------------|
| `EOF`          | -1    | the end of the input, or a failure        |
| `BUFSIZ`       | 512   | the buffer each open file has             |
| `FOPEN_MAX`    | 8     | files open at once, besides the three     |
| `FILENAME_MAX` | 256   | room for a path                           |
| `L_tmpnam`     | 16    | room for a name from `tmpnam`             |
| `TMP_MAX`      | 1000  | how many names `tmpnam` can make          |

`FOPEN_MAX` counts the files `fopen` and `tmpfile` open; the standard
streams are not among them, even after `freopen`. The table of eight
and their buffers are static, not from the heap: about 4 KB, in any
program that opens a file.

## `fopen`

    FILE *fopen(const char *path, const char *mode);

Open the file `path` on the SD card, as MOS names it, and answer a
stream to read or write it, or `NULL` when it cannot be opened: MOS
refused (the file is not there for an `"r"`, or the path is wrong), the
mode is not one of these, or `FOPEN_MAX` files are already open.

| `mode` | Opens                                                 |
|--------|-------------------------------------------------------|
| `"r"`  | to read; the file must be there                       |
| `"w"`  | to write; made, or emptied when it is there           |
| `"a"`  | to write at the end; made when it is not there        |
| `"r+"` | to read and write; the file must be there             |
| `"w+"` | to read and write; made, or emptied                   |
| `"a+"` | to read, and to write at the end                      |

The mode is `r`, `w` or `a`, then any of `+`, and `b` or `t`, in any
order. Anything else in the mode, such as C11's `x`, makes `fopen`
fail.

Without `b` the file is text, as agondev's library has it: each `'\n'`
written is a return and a line feed in the file, and a return followed
by a line feed is read as one `'\n'`. A return with anything else after
it is read as it is. `fwrite` is the exception, and writes its bytes as
they are in either mode. `t` asks for text, which is what no `b` gives
anyway. With `b` the bytes are read and written as they are.

In the `a` modes every write goes at the end of the file, wherever the
place has been put by a seek, and the place starts at the end, as MOS
opens it, so an `"a+"` read starts there too until a seek moves it.

A file is fully buffered: what is written is held in its 512-byte
buffer until the buffer is full, the stream is flushed, sought or
closed, or the program ends. A read takes up to 512 bytes from MOS at a
time. `setvbuf` changes that.

C asks a program to flush or seek between writing a stream and reading
it, or between reading and writing. acc does not need it: output held
is written first, and input read ahead is given back by moving MOS to
where the program has got to.

Every file still open when the program ends (by `exit` or by returning
from `main`, after the `atexit` functions) is flushed and closed.

    FILE *f = fopen("/data/scores.txt", "a");

    if (!f)
        return 1;
    fprintf(f, "%s %d\n", name, score);
    fclose(f);

Each stream has an orientation, as C99 says: the first byte function
used on it makes it a byte stream, and the first wide one (in
`wchar.h`) makes it wide. The byte functions do not refuse a stream
that is already wide.

## `fclose`

    int fclose(FILE *f);

Flush what is held for `f`, close the file, and free its place in the
table. The answer is 0, or `EOF` when the flush failed (the file is
closed anyway), or when `f` is `NULL` or not open. A stream from
`tmpfile` is removed from the card as well.

The keyboard and the screen are not closed: for them the answer is 0
and the stream goes on working.

## `freopen`

    FILE *freopen(const char *path, const char *mode, FILE *f);

Close the file `f` has open, if it has one, and open `path` with `mode`
(as `fopen` takes them) into the same `FILE`. The answer is `f`, or
`NULL` when the open failed, in which case `f` is closed.

It is how `stdin`, `stdout` and `stderr` are sent to a file:

    if (!freopen("out.txt", "w", stdout))
        return 1;
    printf("into the file\n");

A standard stream takes a 512-byte buffer of its own the first time.
Once `stdout` is a file, `printf`, `putchar` and `puts` write to it, and
`puts` writes no return in front of the newline. A `stdout` or
`stderr` made a file is flushed and closed when the program ends.

A `NULL` `path`, which C99 uses to change only the mode, is not
supported: the answer is `NULL` and `f` is left as it was. There is no
way back to the keyboard or the screen; when the open fails for
`stdout`, `printf`, `putchar` and `puts` go to the screen again, but
`fputc` and `fprintf` on `stdout` fail.

## `fflush`

    int fflush(FILE *f);

Write what is held for `f` to MOS. The answer is 0, or `EOF` when the
write failed.

On a stream that was last read, `fflush` gives back what was read ahead
and not used, and forgets a character pushed back by `ungetc`, so MOS is
where the program has got to. (C99 leaves that undefined.)

`fflush(NULL)` flushes every file `fopen` and `tmpfile` opened, and
and `stdout` and `stderr` where `freopen` made them files, and answers
`EOF` when any of them failed. On the keyboard and the screen
`fflush` does nothing and answers 0.

## `setvbuf`, `setbuf`, `_IOFBF`, `_IOLBF`, `_IONBF`

    int setvbuf(FILE *f, char *buf, int mode, size_t size);
    void setbuf(FILE *f, char *buf);

How a file's output is held:

| `mode`   | Value | Output is written                          |
|----------|-------|--------------------------------------------|
| `_IOFBF` | 0     | when the buffer is full                    |
| `_IOLBF` | 1     | after each `'\n'`, or when the buffer is full |
| `_IONBF` | 2     | a character at a time                      |

`buf` and `size` are ignored: each stream already has its own buffer of
`BUFSIZ` bytes, and keeps it. The answer is 0, or -1 when `f` is `NULL`
or `mode` is none of the three. It may be called at any time, not only
before the first read or write.

`setbuf(f, buf)` is `setvbuf` with `_IOFBF` when `buf` is not `NULL`,
and `_IONBF` when it is.

The mode applies to files only. The keyboard is always read a line at a
time and the screen is always unbuffered, whatever `setvbuf` says. An
unbuffered file costs a call into MOS, and a write to the card, for
every character.

## `fgetc`, `getc`, `getchar`

    int fgetc(FILE *f);
    int getc(FILE *f);
    int getchar(void);

The next character of `f` (of `stdin` for `getchar`) as an
`unsigned char`, or `EOF` at the end of the input or on a failure;
`feof` and `ferror` tell which. Reading a stream that was not opened to
read sets its error flag and answers `EOF`. In a text file a return
and the line feed after it are read as one `'\n'`.

`getc` is a function, not a macro, so it is `fgetc` exactly.

## `ungetc`

    int ungetc(int c, FILE *f);

Push `c` (as an `unsigned char`) back onto `f`, to be the next
character read, and take off its end-of-file flag. The answer is `c`.

One character can be pushed back, as C promises, and no more: a second
`ungetc` before the first is read answers `EOF` and pushes nothing. An
`EOF` for `c` pushes nothing and answers `EOF`. A seek, `fflush` or
`rewind` forgets the character pushed back.

## `fgets`, `gets`

    char *fgets(char *s, int n, FILE *f);
    char *gets(char *s);

`fgets` reads characters of `f` into `s` until it has read a `'\n'`
(which it keeps), has read `n - 1`, or comes to the end of the input,
and adds a terminator. The answer is `s`, or `NULL` when nothing was
read, or a read failed (then what is in `s` is not to be used).

An `n` of 0 or less answers `NULL`. An `n` of 1 reads nothing and
answers `s`, holding the empty string.

`gets` reads a line of `stdin` into `s`, without its `'\n'`, and adds a
terminator. The answer is `s`, or `NULL` at the end of the input with
nothing read, or on a failure. Nothing stops it writing past the end of
`s`. C99 has it; C11 took it out. Use `fgets`.

## `fputc`, `putc`, `putchar`

    int fputc(int c, FILE *f);
    int putc(int c, FILE *f);
    int putchar(int c);

Write `c`, as an `unsigned char`, to `f` (to `stdout` for `putchar`),
and answer it; the answer is `EOF` when the write failed, or `f` was
not opened to write (which sets its error flag).

`putc` is a function, not a macro. `'\n'` is written to a text file or
the screen as a return and a line feed, and to a binary file as it
stands, a line feed alone.

## `fputs`, `puts`

    int fputs(const char *s, FILE *f);
    int puts(const char *s);

`fputs` writes `s`, without its terminator, to `f`. `puts` writes `s`
and a newline to `stdout`. The answer is 0, or `EOF` when `fputs` could
not write it all; `puts` always answers 0.

Each `'\n'`, of `s` and the one `puts` adds, goes as `fputc` writes it:
a return and a line feed to the screen or a text file, a line feed alone
to a binary one. `fputs` writes nothing it is not given.

## `fread`, `fwrite`

    size_t fread(void *to, size_t size, size_t count, FILE *f);
    size_t fwrite(const void *from, size_t size, size_t count, FILE *f);

`fread` reads up to `count` items of `size` bytes from `f` into `to`;
`fwrite` writes `count` items from `from` to `f`. Both answer how many
whole items they read or wrote, which is less than `count` only at the
end of the input or on a failure. A `size` or `count` of 0 answers 0
and does nothing.

`fread` from a text file reads a return and the line feed after it as
one `'\n'`, as `fgetc` does. `fwrite` writes the bytes it is given as
they are, in a text file too, as agondev's library does: a block of data
written to a file opened without `b` arrives unchanged. To the screen,
as agondev's does, it writes each `'\n'` as a return and a line feed.

A read from a binary file, or a write, of 512 bytes or more goes
straight between MOS and the program's memory, not through the buffer. A shorter write is
copied into the buffer, and to an unbuffered or line-buffered stream,
or the screen, it is written a character at a time.

`size * count` is worked out in a `size_t` of 24 bits, with no check
that it fits.

## `fseek`, `ftell`, `rewind`, `SEEK_SET`, `SEEK_CUR`, `SEEK_END`

    int fseek(FILE *f, long offset, int whence);
    long ftell(FILE *f);
    void rewind(FILE *f);

`fseek` moves the place in the file `f` is reading or writing to
`offset` bytes from the start (`SEEK_SET`, 0), from where it is
(`SEEK_CUR`, 1) or from the end (`SEEK_END`, 2). It writes what is held
first, forgets a character pushed back, and takes off the end-of-file
flag. The answer is 0, or -1 when `f` is not a file (the keyboard or
the screen), `whence` is none of the three, the flush failed, or the
place would be before the start of the file. A place is a count of the
file's own bytes, in a text file as in a binary one, the returns before
its line feeds included, so any `offset` may be used with any `whence`.

`ftell` answers the place, in the file's bytes from the start,
counting what is held to be written and leaving out what was read
ahead or pushed back. It answers -1 when `f` is not a file.

`rewind(f)` is `fseek(f, 0L, SEEK_SET)`, and takes off the error flag
as well.

The length of a file being written is kept by the library, so
`SEEK_END` is right for a file the program is writing.

## `fgetpos`, `fsetpos`, `fpos_t`

    int fgetpos(FILE *f, fpos_t *pos);
    int fsetpos(FILE *f, const fpos_t *pos);

`fpos_t` is a `long`, the place as `ftell` answers it. `fgetpos` stores
`ftell(f)` in `*pos` and answers 0, or answers -1 and stores nothing
when `ftell` fails. `fsetpos` is `fseek(f, *pos, SEEK_SET)`.

## `feof`, `ferror`, `clearerr`

    int feof(FILE *f);
    int ferror(FILE *f);
    void clearerr(FILE *f);

`feof` answers whether a read has come to the end of `f`'s input, and
`ferror` whether a read or a write on it has failed. `clearerr` takes
both flags off.

An end of file stays until `clearerr`, `ungetc`, a seek or `rewind`
takes it off: a stream at its end answers `EOF` to every read, even
when the file has grown or the keyboard has more to give. An error
stays until `clearerr` or `rewind`.

`feof(NULL)` answers 0; `ferror(NULL)` answers 1.

## `perror`

    void perror(const char *s);

Write the words `strerror` has for `errno` to `stderr`, with `s` and a
colon and a blank in front of them when `s` is not `NULL` or empty, and
a `'\n'` after them.

    errno = ERANGE;
    perror("value");            /* value: Numerical result out of range */

The words are those `errno` held when `perror` was called. No file
function sets `errno`, so the failures to describe this way are those
of the functions that do, such as `strtol`.

## `remove`, `rename`

    int remove(const char *path);
    int rename(const char *from, const char *to);

`remove` deletes the file `path` and `rename` renames `from` to `to`,
through MOS. Both answer 0, or -1 when MOS refused. Neither sets
`errno`.

## `tmpnam`, `tmpfile`

    char *tmpnam(char *s);
    FILE *tmpfile(void);

`tmpnam` answers a name that no file has: `acctmp000.tmp` to
`acctmp999.tmp`, with no directory, so it names a file in MOS's current
directory. It goes on from where the last call stopped, takes the first
name that is not on the card, and answers `NULL` when all `TMP_MAX` are
taken. The name is written to `s`, which needs room for `L_tmpnam`
bytes, or, when `s` is `NULL`, to a static buffer the next such call
writes over. Nothing is made: another program may take the name before
this one does.

`tmpfile` opens a file with such a name, `"wb+"`, and answers it, or
`NULL` when there is no free place in the table or no free name, or
MOS cannot make it. It is removed from the card when it is closed,
which is at the end of the program at the latest; a program that stops
some other way leaves it there.

## `printf`, `fprintf`, `sprintf`, `snprintf`

    int printf(const char *fmt, ...);
    int fprintf(FILE *f, const char *fmt, ...);
    int sprintf(char *to, const char *fmt, ...);
    int snprintf(char *to, size_t n, const char *fmt, ...);

Write `fmt` with each conversion in it replaced by the next argument,
formatted: `printf` to `stdout`, `fprintf` to `f`, and `sprintf` and
`snprintf` into the string at `to`, with a terminator. The answer is how
many characters were written, not counting the terminator, or a
negative number when a file refused one, as a stream opened only to
read does.

`printf` to the screen writes each character as it goes; nothing is
held back. `fprintf` writes through `fputc`, so a file's buffering
applies.

`snprintf` writes at most `n - 1` characters and a terminator, and
answers the length the whole would have had, so an answer of `n` or
more means it was cut. With an `n` of 0 it writes nothing, and `to` may
be `NULL`:

    int len = snprintf(NULL, 0, "%s-%d", name, n);
    char *s = malloc(len + 1);

    snprintf(s, len + 1, "%s-%d", name, n);

`sprintf` has no limit: `to` must have room for all of it.

A conversion is `%`, then flags, a width, a precision, a length, and
the conversion's letter:

| Conversion  | Argument      | Written                              |
|-------------|---------------|--------------------------------------|
| `%d`, `%i`  | `int`         | signed decimal                       |
| `%u`        | `unsigned`    | unsigned decimal                     |
| `%o`        | `unsigned`    | octal                                |
| `%x`, `%X`  | `unsigned`    | hex, in small or capital letters     |
| `%c`        | `int`         | the character, as `unsigned char`    |
| `%s`        | `char *`      | the string                           |
| `%p`        | `void *`      | `0x` and the address in small hex    |
| `%f`, `%F`  | `double`      | `[-]ddd.dddddd`                      |
| `%e`, `%E`  | `double`      | `[-]d.dddddde+dd`                    |
| `%g`, `%G`  | `double`      | `%e` or `%f`, trailing zeros off     |
| `%a`, `%A`  | `double`      | `[-]0x1.hhhp+d`, hex and a power of 2 |
| `%n`        | `int *`       | nothing; stores the count so far     |
| `%%`        |               | a `%`                                |

| Flag    | Meaning                                                 |
|---------|---------------------------------------------------------|
| `-`     | against the left of the field, blanks after             |
| `0`     | zeros, not blanks, after the sign or `0x`               |
| `+`     | a `+` on a signed number that is not negative           |
| (blank) | a blank where that `+` would be                         |
| `#`     | `%o` led by `0`, `%x` by `0x`; floats keep the point,   |
|         | and `%g` its trailing zeros                             |

| Length | `%d`, `%i`    | `%u`, `%o`, `%x`       | `%n`            |
|--------|---------------|------------------------|-----------------|
| `hh`   | `signed char` | `unsigned char`        | `signed char *` |
| `h`    | `short`       | `unsigned short`       | `short *`       |
| `l`    | `long`        | `unsigned long`        | `long *`        |
| `ll`   | `long long`   | `unsigned long long`   | `long long *`   |
| `j`    | `long long`   | `unsigned long long`   | `long long *`   |
| `z`    | `int`         | `unsigned int`         | `int *`         |
| `t`    | `int`         | `unsigned int`         | `int *`         |

`intmax_t` is a `long long`, and `size_t` and `ptrdiff_t` are as wide as
an `int`, which is why `j`, `z` and `t` read those. `L` is read and
ignored: acc has no `long double`.

A width is the least number of characters the field takes; a precision
is the least number of digits for an integer (a 0 written with a
precision of 0 is nothing at all), the digits after the point for `%f`,
`%e` and `%a`, the significant digits for `%g`, and the most characters
of a string to write (a string with a precision need not have a
terminator within it). Either may be `*`, read from an `int` argument:
a negative width is the `-` flag and a positive width, and a negative
precision is as if none had been written. A precision turns the `0`
flag off for an integer.

`%c` and `%s` take only `-` and a width. A `NULL` for `%s` writes
`(null)`. `%lc` and `%ls` take a `wchar_t` (16 bits) and a wide string,
and write them as UTF-8; the width and precision of `%ls` count bytes,
and the precision never cuts a character in two.

`double` in acc is the 4-byte `float`. The floating conversions write
the value exactly, rounded to the precision (6 when none is given) to
the nearest, a tie to the even digit; a precision past 155 is taken
as 155. Infinity is `inf` and not a number is `nan` (`INF` and `NAN` for
the capital conversions), with a sign when it is negative; the `0` flag
pads them with blanks. `%a` without a precision writes no trailing
zeros, and its exponent has as many digits as it needs.

The floating conversions are about 9 KB, and are linked only when some
file of the program passes a `float` or `double` as an argument that a
function's `...` takes. When none does, each of them writes a `?`.

A conversion letter acc does not know is written out as it was, `%` and
the letter, and so is a `%` at the end of `fmt`.

## `vprintf`, `vfprintf`, `vsprintf`, `vsnprintf`

    int vprintf(const char *fmt, va_list ap);
    int vfprintf(FILE *f, const char *fmt, va_list ap);
    int vsprintf(char *to, const char *fmt, va_list ap);
    int vsnprintf(char *to, size_t n, const char *fmt, va_list ap);

`printf`, `fprintf`, `sprintf` and `snprintf` with the arguments in a
`va_list` the caller has started with `va_start`, for a function of its
own that takes `...`:

    void fail(const char *fmt, ...)
    {
        va_list ap;

        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        exit(1);
    }

## `scanf`, `fscanf`, `sscanf`

    int scanf(const char *fmt, ...);
    int fscanf(FILE *f, const char *fmt, ...);
    int sscanf(const char *s, const char *fmt, ...);

Read input as `fmt` describes it, from `stdin`, from `f`, or from the
string `s`, and store each item converted through the next argument,
which points at where it goes. The answer is how many items were
stored, or `EOF` when the input ended (or a read failed) before any
conversion was done.

In `fmt`, white space (blank, tab, newline, vertical tab, form feed,
return) skips any white space in the input, none included. Any other
character must be the next one of the input, or the reading stops
there. `%%` skips white space and then must find a `%`.

A conversion is `%`, then `*` to read the item but store nothing (and
take no argument, and not count it), a width (the most characters to
read), a length, and the conversion's letter. Every conversion but
`%c`, `%[` and `%n` skips white space first.

| Conversion   | Reads                                  | Stores to     |
|--------------|----------------------------------------|---------------|
| `%d`         | a decimal integer, with a sign         | `int *`       |
| `%i`         | an integer: `0x` hex, `0` octal, or    | `int *`       |
|              | decimal, with a sign                   |               |
| `%u`         | a decimal integer, with a sign         | `unsigned *`  |
| `%o`         | an octal integer, with a sign          | `unsigned *`  |
| `%x`, `%X`   | a hex integer, `0x` or not, with sign  | `unsigned *`  |
| `%p`         | a hex integer, as `%x`                 | `void **`     |
| `%f`, `%e`,  | a floating number as `strtod` reads    | `float *`     |
| `%g`, `%a`   | it, and their capitals too             |               |
| `%c`         | exactly width characters (1 when none) | `char *`      |
|              | as they are, with no terminator        |               |
| `%s`         | characters up to white space, and a    | `char *`      |
|              | terminator                             |               |
| `%[...]`     | characters in the set, and a           | `char *`      |
|              | terminator                             |               |
| `%n`         | nothing; stores the characters read    | `int *`       |
|              | so far, and is not counted             |               |

| Length | Integers and `%n`       | Floats     | `%c`, `%s`, `%[` |
|--------|-------------------------|------------|------------------|
| `hh`   | `signed char *`         | `float *`  | `char *`         |
| `h`    | `short *`               | `float *`  | `char *`         |
| `l`    | `long *`                | `double *` | `wchar_t *`      |
| `ll`   | `long long *`           | `double *` | `wchar_t *`      |
| `j`    | `long long *`           | `double *` | `wchar_t *`      |
| `L`    | `long *`                | `double *` | `wchar_t *`      |
| `z`    | `int *`                 | `float *`  | `char *`         |
| `t`    | `int *`                 | `float *`  | `char *`         |

The integer lengths store the same width whether the conversion is
signed or not. `double` is `float`, so `%f` and `%lf` store the same 4
bytes. An integer too big for what it is stored in keeps its low bits;
one past 64 bits is read as the largest 64-bit number, and `errno` is
set to `ERANGE`. A floating number out of a `float`'s range is read as
`strtod` reads it, infinity or zero, and sets `ERANGE` too. A sign is
taken on every integer conversion: `-1` read with `%u` stores
`UINT_MAX`.

A set, `%[...]`, is the characters between the brackets; `^` first
makes it every character that is not among them; a `]` first (or after
the `^`) is in the set rather than its end; and `a-z` is every
character from `a` to `z`. A `-` first or last is itself.

    char key[32];
    int value;

    if (sscanf(line, " %31[^=]= %d", key, &value) == 2)
        set(key, value);

A floating number is read the way `strtod` reads one: a sign, then
`inf`, `infinity`, `nan` or `nan(...)`, in any case, or decimal digits
with a point and an `e` exponent, or `0x` and hex digits with a point
and a `p` exponent. A number is at most 127 characters, and a width
past that is taken as 127.

With `l`, `%c`, `%s` and `%[` store `wchar_t`, and the input's UTF-8 of
two and three bytes is decoded into them.

The input is read one character ahead and that one pushed back, as C
allows: `"1e+x"` read with `%f` takes the `1e+` and fails, and the `x`
is left. `"0x"` read with `%x` or `%i` stores 0, and the `x` is gone.

A conversion letter acc does not know stops the reading, as if the
input did not match.

`scanf` reads `stdin`, so it waits for a line and Enter, and the input
ends when Escape is pressed.

## `vscanf`, `vfscanf`, `vsscanf`

    int vscanf(const char *fmt, va_list ap);
    int vfscanf(FILE *f, const char *fmt, va_list ap);
    int vsscanf(const char *s, const char *fmt, va_list ap);

`scanf`, `fscanf` and `sscanf` with the arguments in a `va_list` the
caller has started with `va_start`.
