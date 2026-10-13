# Using acc

How to compile a program, build one from several files, make a library,
and read what acc says when something is wrong. The options are listed one
by one in [acc's options](options.md).

## Installing

An acc release unzips onto the root of the SD card:

    /bin/acc.bin              the compiler
    /lib/acc/libc.a           the C library
    /lib/acc/rt.a             acc's runtime, which every program calls
    /lib/acc/include/         its headers, stdio.h to agon/vdp.h

acc finds its headers and libraries there without being told, and MOS
finds `acc` in `/bin` from any directory.

## Compiling a program

A source is compiled and linked in one step. The program is named after
the source, `hello.bin` here, and runs like any other MOS command:

    */ acc hello.c
    */ hello Agon
    hello, Agon

`-o` gives it another name: `acc hello.c -o greet.bin`. Every compile and
link ends with `Done in 0.04 seconds`.

## `-c`, objects, linking

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

## `-a`, libraries

`-a` collects objects into a library. A link takes from it only the
members a program uses:

    */ acc -a mylib.a util.o parse.o
    */ acc main.c mylib.a -o prog.bin

## `@`, long command lines

MOS cuts a command line at about 255 characters. A command longer than
that reads the rest of its words from a file named with `@`, in the
file's place. Words are separated by spaces, tabs or line ends, and
`"..."` is one word:

    */ type prog.lnk
    main.o util.o parse.o
    lex.o
    */ acc @prog.lnk -o prog.bin

## `-I`, headers

`#include "x.h"` looks beside the file that includes it, then in each
`-I` directory in the order given, then in `/lib/acc/include`.
`#include <x.h>` skips the first. Up to 8 `-I` directories may be given.

    */ acc -c main.c -I /src/common

## `-moslet`, moslets

A moslet is a program MOS runs from `/mos`, loaded at `0x0B0000` rather
than `0x040000`, so it can run while another program is in memory, and is
found before anything in `/bin`:

    */ acc hello.c -moslet -o /mos/hello.bin

A moslet has 32 KB for everything, from `0x0B0000` to `0x0B8000`: its
code, its variables, a 16 KB stack and what is left as heap. acc says so
when a program does not fit.

## Errors, `-errors`

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

## What `main` returns

What `main` returns, or `exit` is given, goes back to MOS in HL, as from
a program agondev built. `-p` prints it as six hex digits, and `-x`
writes it to IO port 0, which stops an emulator with it as the exit
status.

## Output to a file

What acc prints can be sent to a file as any MOS command's can:
`acc main.c > log.txt`, or `>>` to add to it.

## `-v`, `-h`

`acc -v` prints the version; `acc -h` lists the options on one screen;
`acc` alone prints a short summary of how to call it.
