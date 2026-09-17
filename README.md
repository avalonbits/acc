# acc - a C compiler for the Agon

A C compiler written for the Agon Light: one that runs **on** the machine,
in the 448 KB it gives a program for code, data, heap and stack together.

It is early, and it is being written towards C99. What works today:

  * `char`, `short`, `int` and the `unsigned` form of each, at agondev's
    widths -- 1, 2 and 3 bytes -- so a program compiled by either comes out
    the same
  * functions, parameters, locals, calls, recursion, forward references
  * `+ - * / %`, unary minus, `~`, `& | ^ << >>`, and all six comparisons,
    signed or unsigned according to the operands
  * `if`, `else`, `else if`, `while`
  * both comment forms

One `.c` file in, a runnable MOS binary out: no linker, no preprocessor and
no library. `* / % & | ^ << >>` have no eZ80 instruction on a 24-bit value, so
acc carries those routines and emits the ones a program uses into that
program's image.

    $ cat t.c
    int add(int a, int b) { return a + b; }
    int main(void) { int x = 3; return add(x, 4) + 35; }

    $ bin/acc t.c -o t.bin
    $ test/agon.sh t.bin ; echo $?
    42

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

What is not kept is tinycc's memory: a symbol here is 7 bytes, names live in
one arena that is never freed, and the output is a flat MOS image written
directly rather than ELF sections assembled and then relocated.

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
| the operators the chip lacks: `* / % & \| ^ << >>` | next |
| `if`, `while`, `for`, comparisons | |
| globals, `char`, `short`, pointers, arrays | |
| a preprocessor | |
| `struct`, `union`, `enum`, `typedef` | |
| linking against libagon, so there is a C library | |
