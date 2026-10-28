# acc's options

    acc [-c] <source.c> [<file.o|lib.a>]... [options]
    acc <file.o|lib.a>... [options]
    acc -a <lib.a> <file.o>...

Options may come before or after the files, in any order. How they fit
together is in [Using acc](using.md).

## `-c`

Compile the source to an object, to be linked later, and do not link.
The object is the source's name with `.o`, unless `-o` names it. An
object whose source, headers and options have not changed is not made
again: `main.o is up to date`.

## `-o`

    -o <file>

What to write: the program, the object with `-c`, or nothing else. Without
it, the first input's name with `.bin`, or `.o` with `-c`.

## `-a`

    -a <lib.a> <file.o>...

Put the objects after it in the library `<lib.a>`, made new. A link
takes from a library only the members a program uses.

## `-I`

    -I <dir>

Look in `<dir>` for an `#include`, after the directory of the file that
includes it and before `/lib/acc/include`. Directories are searched in the
order given; there may be 8.

## `-D`, `-U`

    -D <name>[=<value>]
    -U <name>

Define a macro, as `#define name value` at the top of the source would;
`-D name` alone is `name` as 1. `-U` undefines one, a predefined macro
included. Together there may be 64 of them. `-Dname` and `-Uname`, with no
space, are the same.

## `-include`

    -include <file>

Read `<file>` before the source, as if it were included on its first line.

## `-b`

    -b <hex>

Load the program at the address `<hex>`, in hexadecimal, rather than at
`40000`.

## `-moslet`

Make a moslet, for `/mos`: loaded at `B0000`, with the 32 KB up to
`B8000` for code, variables, heap and stack. acc says so when a program
does not fit.

## `-r`

    -r <file>

Write to `<file>` the offsets inside the image of every address `-b` moved,
for a loader that puts the program somewhere else.

## `-map`

    -map <file>

Write to `<file>` where each function and variable went, a line each.
With `-c` it is the object's items, `name offset size`, `static` ones
included. A link of objects writes `address size name object offset`,
in the image's order, with the runtime's routines last. A source compiled
and linked in one step writes no map: compile it with `-c` and link the
object to have one.

## `-p`

Make the program print what `main` returned, as six hex digits.

## `-x`

Make the program write what `main` returned to IO port 0, which stops an
emulator with it as the exit status.

## `-trigraphs`

Read `??(` and the other eight trigraphs as C99 has them. Without it they
are left as they are written.

## `-errors`

    -errors <file>

When there is an error, write it to `<file>` as well, as one line
`file:line:column: error: text`, and fail with 100. A compile that works
removes the file.

## `@`

    @<file>

Read more arguments from `<file>`, in its place: words separated by
spaces, tabs or line ends, `"..."` one word. For a command longer than
MOS's 255 characters, such as a link of many objects.

## `-v`, `--version`

Print the version and the build, and stop: `acc 0.6.2 (build 4099300)`.

## `-h`, `--help`

List the options on one screen, and stop.
