# acc - a C compiler for the Agon

acc is a C compiler that runs on the Agon and produces MOS programs. It
also builds and runs on Linux, where it produces the same programs and
objects byte for byte.

It compiles C99 apart from `long double`, `_Complex`, and function
definitions in the K&R style, and it comes with a C library of its own:
every header of C99's hosted library, plus agondev's `<agon/...>` headers
for MOS, the VDP, the keyboard, GPIO, joysticks and timers.

## Installing on the Agon

Unzip `acc-<version>.zip` onto the root of the SD card. It holds:

    /bin/acc.bin              the compiler
    /lib/acc/libc.a           the C library
    /lib/acc/rt.a             acc's runtime, which every program calls
    /lib/acc/include/         its headers, <stdio.h> to <agon/vdp.h>

acc looks in `/lib/acc/include` for every `#include`, and takes what a
program calls from `/lib/acc/rt.a` and `/lib/acc/libc.a`, without being
told. `/bin` is where
MOS looks for a command, so `acc` works from any directory. It is tested
on MOS 3.0.2.

## Using it on the Agon

A program is compiled from its source in one step, and runs like any other
MOS command. The output is named after the source, `hello.bin` here:

    */ type hello.c
    #include <stdio.h>

    int main(int argc, char **argv)
    {
        printf("hello, %s\n", argc > 1 ? argv[1] : "world");
        return 0;
    }
    */ acc hello.c
    */ hello Agon
    hello, Agon

`-o` names the output something else: `acc hello.c -o greet.bin`.

A program in several files is compiled a file at a time with `-c`, which
writes `main.o` from `main.c`, and linked together. The link is named after
its first input, `main.bin` here. An object records the files it was made
from, so a second `-c` over unchanged sources does nothing:

    */ acc -c main.c
    */ acc -c util.c
    */ acc main.o util.o
    */ acc -c main.c
    main.o is up to date

The steps go well in an obey file, which MOS runs a line at a time. Here is
a program in two files and the script that builds it:

    */ type util.h
    int square(int n);
    */ type util.c
    #include "util.h"

    int square(int n)
    {
        return n * n;
    }
    */ type main.c
    #include <stdio.h>
    #include "util.h"

    int main(void)
    {
        printf("7 squared is %d\n", square(7));
        return 0;
    }
    */ type build.obey
    acc -c main.c
    acc -c util.c
    acc main.o util.o -o prog.bin
    */ obey build.obey
    */ prog
    7 squared is 49

So the script can be run after every edit: `acc -c` compiles a file again
only if something it was made from has changed, and says so when nothing
has. After editing only `main.c`:

    */ obey build.obey
    util.o is up to date

`main.c` is compiled and the program linked again; `util.c` is not
compiled. What counts is the source, every header it includes, and the
`-D`, `-U` and `-I` options given. A change means different contents, not
a newer date, so saving a file unchanged compiles nothing.

A source can also be named with objects and libraries, which are linked
after it:

    */ acc main.c util.o

Objects can be collected into a library, from which a link takes only the
members a program uses:

    */ acc -a mylib.a util.o parse.o
    */ acc main.c mylib.a -o prog.bin

Headers of your own are found beside the file that includes them, or in a
directory given with `-I`, which is searched before `/lib/acc/include`:

    */ acc -c main.c -I /src/common

`acc -v` prints the version:

    */ acc -v
    acc 0.2.0 (build 440616)

Every compile and link ends by printing the time it took, as
`Done in 0.04 seconds`; the examples leave that line out. An error prints
as `file:line:column: error: text`, and acc stops at the first one.

What acc prints can be sent to a file, as a MOS command's can: `acc main.c
> log.txt`, or `>>` to add to it. A command line may be as long as MOS
takes, so a program of dozens of objects links in one command.

## Options

    acc [-c] <source.c> [<file.o|lib.a>]... [-o <out>] [-I <dir>]...
             [-D <name>[=<value>]]... [-U <name>]... [-include <file>]
    acc <file.o|lib.a>... [-o <out.bin>]
    acc -a <lib.a> <file.o>...
    acc -v
    acc -h

| Option | Meaning |
| --- | --- |
| `-c` | Compile to an object, to be linked later |
| `-o <out>` | The program or object to write; without it, the first input's name with `.bin`, or `.o` with `-c` |
| `-a <lib.a>` | Put the objects that follow into a library |
| `-I <dir>` | Look in `<dir>` for an `#include` |
| `-D <name>[=<value>]` | Define a macro, as `#define` would; `-DN` is `-DN=1` |
| `-U <name>` | Undefine a macro |
| `-include <file>` | Read `<file>` before the source |
| `-p` | Print what `main` returned, as six hex digits |
| `-x` | Write what `main` returned to IO port 0, which stops an emulator with it as the exit status |
| `-b <addr>` | Load the program at `<addr>`, in hexadecimal; the default is `40000` |
| `-r <file>` | Write the offsets inside the image that `-b` moved |
| `-map <file>` | Write where each function and variable went |
| `-trigraphs` | Read the nine trigraphs |
| `-errors <file>` | Also write an error to `<file>`, and fail with 100 |
| `-v`, `--version` | Print the version and exit |
| `-h`, `--help` | List the options, in one screen |

`acc` on its own prints a short summary of how to use it.

What `main` returns goes back to MOS, as it does from a program agondev
built.

When acc fails on the Agon, what it prints is all there is: it returns a
code MOS has no message for, so MOS adds nothing, and an obey file stops at
the line that failed. A command line acc does not take is MOS's "Invalid
parameter". On the host an error is 1, and a command line 2.

## The language

- Every scalar type at agondev's widths: `char` 1 byte, `short` 2, `int` 3,
  `long` 4, `long long` 8, `float` and `double` both 4-byte IEEE 754
  single, `_Bool`, and pointers of 3 bytes.
- Every operator and statement, blocks with declarations anywhere,
  functions, prototypes, pointers to functions, and variable arguments.
- `struct`, `union`, `enum`, `typedef`, bit-fields, flexible array
  members, arrays of any dimension including variable-length ones,
  designated initialisers, compound literals, and structs passed, returned
  and assigned by value.
- `static`, `extern`, `const`, `volatile`, `register`, `auto`, `inline`
  and `restrict`.
- The whole C99 preprocessor, plus `#pragma once`.
- `__attribute__` is read and ignored.

How it stands against the conformance suites is in
[docs/c99-status.md](docs/c99-status.md).

## How it works

acc compiles in one pass: the parser emits eZ80 code as it reads, with no
syntax tree, and a program's image goes to the card as it is made rather
than being held in memory. acc's own sources compile and link on the Agon.

- [docs/DESIGN.md](docs/DESIGN.md) walks through the compiler: its parts,
  how a source becomes a program, and how it fits in the Agon's memory.
- [docs/OPTIMIZATIONS.md](docs/OPTIMIZATIONS.md) describes each
  optimization acc makes in the code it generates.

## What the code costs

acc's output against agondev's at `-Oz`, on an emulated Agon, as acc's
size or time over agondev's:

| | code | whole image | time to run |
| --- | --- | --- | --- |
| the benchmark programs, mean | 1.33 | 1.06 | 1.18 |
| zap, the assembler | 1.22 | 1.20 | 1.58 |
| acc itself | 1.24 | 1.25 | |

`test/size.sh` and `test/perf.sh` produce these tables; each program's
figures are in [docs/performance.md](docs/performance.md).

On the Agon, acc compiles about 30 KB of C a second.

## Building

On Linux, with a C compiler and make, and a checkout of
[zap](https://github.com/avalonbits/zap) in `~/code/zap` (or
`ZAP_SRC=<dir>`), which assembles acc's runtime into the library:

    make                # bin/acc, bin/zap, bin/libc.a and bin/rt.a
    make test           # the test suites

acc on the host links `bin/rt.a` and `bin/libc.a` by default and has no default include
directory, so name it:

    bin/acc hello.c -Iinclude

The Agon build needs [AgonDev](https://github.com/AgonPlatform/agondev) in
`~/agondev` (or `AGONDEV=<dir>`):

    make -f Makefile.agon       # bin/acc.bin
    ./mkrelease.sh 0.2.0        # acc-0.2.0.zip, the SD card layout

`make test` runs every program it compiles on
[fab-agon-emulator](https://github.com/tomm/fab-agon-emulator), and checks
each answer against agondev's build of the same source.

## License

acc is free software under the GNU Lesser General Public License, version 2.1
or (at your option) any later version; see `COPYING`.

The routines in `src/rt/` -- the startup stub and the arithmetic helpers --
are copied into every program acc compiles, so they carry an exception, the
one glibc gives its own startup code: you may link them into your programs
and distribute those programs without any restriction coming from them. A
program does not take on acc's license by being compiled by it. The LGPL
still covers the routines themselves, if you modify or distribute them on
their own; the full text is at the top of each file.
