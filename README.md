# acc - a C compiler for the Agon

A C compiler written for the Agon Light: one that runs **on** the machine,
in the 448 KB it gives a program for code, data, heap and stack together.

It is early. What works today is straight-line integer code -- functions,
parameters, locals, `+`, `-`, unary minus, `~`, assignment and calls -- and it
runs on the Agon.

    $ cat t.c
    int add(int a, int b) { return a + b; }
    int main(void) { int x = 3; return add(x, 4) + 35; }

    $ bin/acc t.c -o t.bin
    $ test/agon.sh t.bin ; echo $?
    42

## Why not tinycc

There was a tinycc port first, and it works -- it compiles and links on the
Agon. It lives in `old-acc/`, and what it is good for now is answering
questions: it is a known-correct implementation of everything this one does
not do yet.

What it could not do is fit. Measured on the machine, a 200-line source
compiles in about 8 seconds and a 400-line source does not finish at all: its
peak is 235 KB against 206 KB of heap and stack. The reason is its data
structures rather than its algorithms -- 31 bytes for every symbol, and 75 KB
of machinery for a preprocessor -- which is not something to fix by tuning.
`old-acc/docs/porting-notes.md` has the measurements.

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
