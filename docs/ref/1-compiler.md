# The compiler

How to run acc, every option it takes, and the language it compiles: its
types, what it leaves out, and its limits. The groups after this one are
acc's library, a header at a time, C99's and the Agon's. Look any name up
with `ref acc <name>`: a function, a macro, a type, a header or an option.

## Using acc

How to compile a program, build one from several files, make a library,
and read what acc says when something is wrong. The options are listed one
by one in [acc's options](#accs-options).

### Installing

An acc release unzips onto the root of the SD card:

    /bin/acc.bin              the compiler
    /lib/acc/libc.a           the C library
    /lib/acc/rt.a             acc's runtime, which every program calls
    /lib/acc/include/         its headers, stdio.h to agon/vdp.h

acc finds its headers and libraries there without being told, and MOS
finds `acc` in `/bin` from any directory.

### Compiling a program

A source is compiled and linked in one step. The program is named after
the source, `hello.bin` here, and runs like any other MOS command:

    */ acc hello.c
    */ hello Agon
    hello, Agon

`-o` gives it another name: `acc hello.c -o greet.bin`. Every compile and
link ends with `Done in 0.04 seconds`.

### `-c`, objects, linking

A program in several files is compiled a file at a time with `-c`, which
writes `main.o` from `main.c`, and then linked. The program is named after
the first input, `main.bin` here:

    */ acc -c main.c
    */ acc -c util.c
    */ acc main.o util.o

A source can be given with objects and libraries, which are linked after
it: `acc main.c util.o`.

An object records what it was made from: the source, every header it
included, and the `-D`, `-U` and `-I` options. `acc -c` over a source
none of those has changed in does nothing and says so:

    */ acc -c util.c
    util.o is up to date

A change means different contents, not a newer date. So a build script
can compile everything every time, and only what changed is compiled
again:

    */ type build.obey
    acc -c main.c
    acc -c util.c
    acc main.o util.o -o prog.bin
    */ obey build.obey

A link takes from the libraries only the functions a program reaches, and
from them only what they reach in turn: a program that calls `printf`
carries no file functions, and one that prints no float carries none of
the floating conversions.

### `-a`, libraries

`-a` collects objects into a library. A link takes from it only the
members a program uses:

    */ acc -a mylib.a util.o parse.o
    */ acc main.c mylib.a -o prog.bin

### `@`, long command lines

MOS cuts a command line at about 255 characters. A command longer than
that reads the rest of its words from a file named with `@`, in the
file's place. Words are separated by spaces, tabs or line ends, and
`"..."` is one word:

    */ type prog.lnk
    main.o util.o parse.o
    lex.o
    */ acc @prog.lnk -o prog.bin

### `-I`, headers

`#include "x.h"` looks beside the file that includes it, then in each
`-I` directory in the order given, then in `/lib/acc/include`.
`#include <x.h>` skips the first. Up to 8 `-I` directories may be given.

    */ acc -c main.c -I /src/common

### `-moslet`, moslets

A moslet is a program MOS runs from `/mos`, loaded at `0x0B0000` rather
than `0x040000`, so it can run while another program is in memory, and is
found before anything in `/bin`:

    */ acc hello.c -moslet -o /mos/hello.bin

A moslet has 32 KB for everything, from `0x0B0000` to `0x0B8000`: its
code, its variables, a 16 KB stack and what is left as heap. acc says so
when a program does not fit.

### Errors, `-errors`

An error prints as `file:line:column: error: text`, as gcc prints one,
and acc stops at the first. An error with no line, such as a link that
cannot find a function, is `acc: error: text`.

On the Agon a failed compile exits with 100, which MOS has no message
for, so it adds nothing to acc's, and an obey file stops at the line that
failed. A command line acc does not take exits with 19, which MOS prints
as "Invalid parameter". On the host an error is 1 and a command line 2.

`-errors <file>` also writes the error to `<file>`, as one line
`file:line:column: error: text` (the column 0 when it is not known, and
`acc:0:0:` for an error with no line), and makes acc fail with 100 on the
host too. A compile that works removes the file, so a stale error is
never read. It is for a program that runs acc and must know what went
wrong, such as an editor that jumps to the line: MOS cannot redirect one
program's output into another.

### What `main` returns

What `main` returns, or `exit` is given, goes back to MOS in HL, as from
a program agondev built. `-p` prints it as six hex digits, and `-x`
writes it to IO port 0, which stops an emulator with it as the exit
status.

### Output to a file

What acc prints can be sent to a file as any MOS command's can:
`acc main.c > log.txt`, or `>>` to add to it.

### `-v`, `-h`

`acc -v` prints the version; `acc -h` lists the options on one screen;
`acc` alone prints a short summary of how to call it.

## acc's options

    acc [-c] <source.c> [<file.o|lib.a>]... [options]
    acc <file.o|lib.a>... [options]
    acc -a <lib.a> <file.o>...

Options may come before or after the files, in any order. How they fit
together is in [Using acc](#using-acc).

### `-c`

Compile the source to an object, to be linked later, and do not link.
The object is the source's name with `.o`, unless `-o` names it. An
object whose source, headers and options have not changed is not made
again: `main.o is up to date`.

### `-o`

    -o <file>

What to write: the program, the object with `-c`, or nothing else. Without
it, the first input's name with `.bin`, or `.o` with `-c`.

### `-a`

    -a <lib.a> <file.o>...

Put the objects after it in the library `<lib.a>`, made new. A link
takes from a library only the members a program uses.

### `-I`

    -I <dir>

Look in `<dir>` for an `#include`, after the directory of the file that
includes it and before `/lib/acc/include`. Directories are searched in the
order given; there may be 8.

### `-D`, `-U`

    -D <name>[=<value>]
    -U <name>

Define a macro, as `#define name value` at the top of the source would;
`-D name` alone is `name` as 1. `-U` undefines one, a predefined macro
included. Together there may be 64 of them. `-Dname` and `-Uname`, with no
space, are the same.

### `-include`

    -include <file>

Read `<file>` before the source, as if it were included on its first line.

### `-b`

    -b <hex>

Load the program at the address `<hex>`, in hexadecimal, rather than at
`40000`.

### `-moslet`

Make a moslet, for `/mos`: loaded at `B0000`, with the 32 KB up to
`B8000` for code, variables, heap and stack. acc says so when a program
does not fit.

### `-r`

    -r <file>

Write to `<file>` the offsets inside the image of every address `-b` moved,
for a loader that puts the program somewhere else.

### `-map`

    -map <file>

Write to `<file>` where each function and variable went, a line each.
With `-c` it is the object's items, `name offset size`, `static` ones
included. A link of objects writes `address size name object offset`,
in the image's order, with the runtime's routines last. A source compiled
and linked in one step writes no map: compile it with `-c` and link the
object to have one.

### `-p`

Make the program print what `main` returned, as six hex digits.

### `-x`

Make the program write what `main` returned to IO port 0, which stops an
emulator with it as the exit status.

### `-trigraphs`

Read `??(` and the other eight trigraphs as C99 has them. Without it they
are left as they are written.

### `-errors`

    -errors <file>

When there is an error, write it to `<file>` as well, as one line
`file:line:column: error: text`, and fail with 100. A compile that works
removes the file.

### `@`

    @<file>

Read more arguments from `<file>`, in its place: words separated by
spaces, tabs or line ends, `"..."` one word. For a command longer than
MOS's 255 characters, such as a link of many objects.

### `-v`, `--version`

Print the version and the build, and stop: `acc 0.5.0 (build 3886519)`.

### `-h`, `--help`

List the options on one screen, and stop.

## The language

acc compiles C99, at agondev's widths, so that a program means the same
built by either. What it leaves out, and how far it goes, is below.

### Types, `sizeof`

| Type                 | Bytes | Range                                     |
|----------------------|-------|-------------------------------------------|
| `char`               | 1     | -128 to 127: `char` is signed             |
| `short`              | 2     | -32768 to 32767                           |
| `int`                | 3     | -8388608 to 8388607                       |
| `long`               | 4     | -2147483648 to 2147483647                 |
| `long long`          | 8     | 64 bits                                   |
| `float`, `double`    | 4     | IEEE 754 single, both                     |
| `_Bool`              | 1     | 0 or 1                                    |
| a pointer            | 3     | the eZ80's 24-bit address                 |

`size_t` is `unsigned int` and `ptrdiff_t` is `int`, 3 bytes each;
`wchar_t` is `short`. The bytes are little-endian, and a struct has no
padding: every member is at the byte after the one before it.

### What C99 is there

Every operator and statement; declarations anywhere in a block; functions,
prototypes, pointers to functions and variable arguments (`stdarg.h`);
`struct`, `union`, `enum`, `typedef`, bit-fields and flexible array
members; arrays of any dimension, variable-length ones included;
designated initialisers and compound literals; structs passed, returned
and assigned by value; `static`, `extern`, `const`, `volatile`,
`register`, `auto`, `inline` and `restrict`; `__func__`; `//` comments;
and the whole preprocessor.

### `long double`, `_Complex`

`long double` is refused: agondev's library has no arithmetic for one, so
a program that used it could not be linked by either. `_Complex` is
refused too. The `l`-suffixed functions of `math.h` are not there.

### Old-style definitions

A function defined the old way, with its parameters' types after the
parentheses, is refused: `int f(a) int a; { ... }`. So is a call with
arguments to a function defined with an empty `()`.

### `__attribute__`, GNU C

`__attribute__((...))` is read and ignored wherever it is written. acc
says it is GNU C 4.2.1 (`__GNUC__` is 4), as agondev's clang does, so a
program that asks before writing an attribute gets to write it.

Of GNU C's extensions, acc has `__builtin_offsetof` (what `offsetof`
is), `__builtin_unreachable`, `__builtin_ffs` and `__builtin_popcount`.
Statement expressions `({ })`, `typeof`, case ranges `case 1 ... 3`,
labels as values `&&label`, zero-length arrays, binary constants `0b101`
and `asm` are not: each is an error.

### `#pragma`

`#pragma once` reads the file once however often it is included.
`#pragma weak name` makes a reference to `name` no reason to link it: if
nothing else wants it, its address is 0. Any other pragma is ignored, as
C says.

### `__STDC_VERSION__`, predefined macros

| Macro                | Value                                       |
|----------------------|---------------------------------------------|
| `__STDC__`           | 1                                           |
| `__STDC_VERSION__`   | `199901L`                                   |
| `__STDC_HOSTED__`    | 0: the library is not yet all of C99's      |
| `__GNUC__`           | 4 (`__GNUC_MINOR__` 2, `__GNUC_PATCHLEVEL__` 1) |
| `__VERSION__`        | `"acc"` and its version                     |
| `__CHAR_BIT__`       | 8                                           |
| `__SIZEOF_INT__`     | 3, and the other `__SIZEOF_` sizes          |
| `__INT_MAX__`        | 8388607, and the other `_MAX__` limits      |
| `__INT24_TYPE__`     | `int`, and the other `__INTn_TYPE__` names  |
| `__BYTE_ORDER__`     | `__ORDER_LITTLE_ENDIAN__`                   |

The values are agondev's. A program may `#undef` any of them. `__FILE__`,
`__LINE__`, `__DATE__` and `__TIME__` are as C99 has them.

### Limits

| What                                  | The most                         |
|---------------------------------------|----------------------------------|
| `#include` nested                     | 8 deep                           |
| `#if` nested, includers' ones counted | 16 deep                          |
| a macro's parameters and arguments    | 127                              |
| macros expanding inside each other    | 8 deep                           |
| `-I` directories                      | 8                                |
| `-D` and `-U` options                 | 64                               |
| levels of pointer in one type         | 7 (`int *******`)                |
| an array's dimensions                 | 12                               |
| bit-field shapes in a program         | 255                              |
| array shapes and structs in a program | 255                              |
| names in one compile, keywords too    | 12,287                           |

Beyond these, what limits a compile on the Agon is memory: acc has about
175 KB of heap for one source, which a file of about 1,000 functions
fills. A program too big for one file is compiled as several with `-c`.
acc stops with `out of memory for ...` when it runs out, and says what
it was making.

A program's own stack is the 16 KB between its heap and the top of
memory, and nothing stops it running into the heap. Keep large arrays
`static` rather than local.
