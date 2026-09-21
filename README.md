# acc - a C compiler for the Agon

A C compiler written for the Agon Light: one that runs **on** the machine,
in the 448 KB it gives a program for code, data, heap and stack together.

It is being written towards C99. What works today:

  * every scalar type at agondev's widths, so a program compiled by either
    comes out the same: `char`, `short`, `int` and their `unsigned` forms at
    1, 2 and 3 bytes; `long` at 4; `long long` at 8; `float` and `double`,
    which are both the same 4-byte IEEE 754 single there; `_Bool`; and
    pointers, seven deep
  * every operator C has, including `?:`, `&&`, `||`, the comma operator,
    the compound assignments, `++` and `--`, `sizeof` and casts, at every
    width -- and the constants among them are worked out by the compiler,
    whatever their type, so a global may be initialised with any of them
  * `if`, `while`, `for`, `do`, `switch`, `break`, `continue`, `goto` and
    labels, and blocks with declarations anywhere in them
  * functions, prototypes, recursion, forward references, pointers to
    functions, and variable arguments with `va_list`
  * `struct`, `union`, `enum`, `typedef`, bit-fields, flexible array
    members, arrays of any dimension -- including ones whose length the
    program works out -- initialisers with braces, designated initialisers
    (`{ .x = 1 }`, `{ [2] = 7 }`), compound literals, and structs passed,
    returned and assigned by value
  * `static`, `extern`, `const`, `volatile`, `register`, `auto` and
    `inline`, with `const` checked through pointers, elements and members,
    and `static` and the qualifiers inside a parameter's brackets
  * string and character literals with their escapes, `__func__`,
    `_Static_assert`, `__builtin_offsetof`, `__attribute__` (read and
    thrown away: there is no optimiser here for it to advise), and both
    comment forms

Of the preprocessor there is `#include` -- a quoted name is looked for
beside the file that asked for it and then in the `-I` directories, an
angled one only in those -- and `#define` and `#undef` for a name standing
for some text or taking parameters, with `#` and `##` and a variable number
of arguments, and the conditionals -- `#if` and `#elif` on an expression,
with `defined`, as well as `#ifdef`, `#ifndef`, `#else` and `#endif` --
`#error`, `#pragma` and `#line`, and `__FILE__` and `__LINE__`. That is the
whole of it. What is not here is `long double`, because libagon has no
arithmetic for a double at all and half a type is worse than none; nor wide
characters and strings, `_Complex`, or function definitions written the way
K&R wrote them.

One `.c` file in, whatever it includes, a runnable MOS binary out.
`* / % & | ^ << >>` have no eZ80 instruction on a 24-bit value, and nothing
wider than a register has one at all, so acc carries those routines and emits
the ones a program uses into that program's image.

    $ cat t.c
    int add(int a, int b) { return a + b; }
    int main(void) { int x = 3; return add(x, 4) + 35; }

    $ bin/acc t.c -o t.bin
    $ test/agon.sh t.bin ; echo $?
    42

Or in pieces: `-c` compiles one file to an object, `-a` puts objects into a
library, and naming objects and libraries instead of a source links them. An
object records what it was built from, so a second `-c` over an unchanged
source says so and does nothing.

    $ bin/acc -c t.c -o t.o
    $ bin/acc t.o bin/libc.a -o t.bin

The library that comes with it is `bin/libc.a`, written in C and compiled by
acc: `<string.h>`, `<stdlib.h>` with a heap and `qsort`, `<stdio.h>` with
`printf`, `<stddef.h>`, `<stdint.h>`, `<stdbool.h>`, `<time.h>`, and
`<agon/mos.h>`, `<agon/vdp.h>` and `<agon/keyboard.h>` for the machine
itself. A program's `main` is handed the command line as `argc` and `argv`.

## Why not tinycc

There was a tinycc port first, and it worked: it compiled and linked on the
Agon. What it could not do is fit.

Measured on the machine, it compiled a 200-line source in about 8 seconds and
did not finish a 400-line one at all -- its peak was 235 KB against the 206 KB
of heap and stack it had. The cause was its data structures rather than its
algorithms: 31 bytes for every symbol, and 75 KB of machinery for a
preprocessor that had to be resident whether or not a program used one. That
is not something tuning fixes.

This compiler does 416 lines in under a second and its image is 28 KB, which
leaves 439 KB for the heap and the stack. The port has been deleted; it was
kept for a while as a known-correct reference, and the differential tests
against agondev do that job better.

## How it works

One pass. No syntax tree, no intermediate representation. The parser emits
eZ80 machine code as it reads, with a small stack of *descriptions* of values
-- a constant, a local at a frame offset, something already in a register --
that are only turned into instructions when something needs them. That is
tinycc's model, and it is kept because it is both the fastest way to compile C
and the smallest.

What is not kept is tinycc's memory: a symbol here is 10 bytes and a type is
one, names live in one arena that is never freed, and the output is a flat
MOS image written directly rather than ELF sections assembled and then
relocated.

## What the code costs

zap, the assembler this compiler is meant to build on the machine, is twelve
files and 7,900 lines of C. acc compiles and links all of it, and what it
produces assembles zap's own test sources to the byte that agondev's build
of the same source produces. Against that build, at `-Oz`, on an emulated
Agon:

| | agondev | acc | |
| --- | --- | --- | --- |
| the image | 84,893 bytes | 168,582 bytes | 2.0x |
| assembling a 6 KB source, six runs | 0.64s | 3.12s | 4.9x |

Both figures are the assembler's own, taken alternately in one boot so that
neither side pays for a warm-up the other did not.

The size is the one that bites, and not because of the disc. A program gets
448 KB for everything, so every byte of image is a byte of heap: zap has
about 250 KB to allocate from where the reference build has about 360 KB.

It used to be 310,820 bytes and 113 KB, which was not enough to assemble a
25 KB source at all. What closed most of that gap was leaving things out
rather than emitting them better -- the functions a file does not use, which
a header full of `static inline` helpers has a great many of, and the value
a comparison makes when all that was wanted was the branch.

## Testing, without an oracle

zap could be checked against ez80asm byte for byte. Two C compilers may emit
different code and both be right, so there is no such oracle here.

There is a reference *answer*. Every test is compiled twice -- once with acc,
once with agondev -- run on an emulated Agon, and required to report the same
result. That pins the semantics this target actually has, like where a 24-bit
`int` wraps, to the implementation that defines them.

A program reports by writing a byte to IO port 0, which the emulator turns
into its exit status. No C library, no linker, nothing to print with.

    make test

## Where it is going

Each step is meant to be finished and measured before the next one starts.

| | |
| --- | --- |
| functions, `int`, `+ - ~`, calls, locals | done |
| the operators the chip lacks: `* / % & \| ^ << >>` | done |
| `if`, `while`, `for`, comparisons | done |
| globals, `char`, `short`, pointers, arrays | done |
| `struct`, `union`, `enum`, `typedef` | done |
| `long`, `float`, `long long`, `_Bool` | done |
| bit-fields, pointers to functions, `...` and `va_list` | done |
| designated initialisers, compound literals, arrays with a length worked out | done |
| a preprocessor | done |
| objects, a linker, and a C library of its own | done |
| compiling itself on the machine | next |

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
