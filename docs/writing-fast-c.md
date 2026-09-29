# Writing C that acc compiles well

acc has no optimisation levels: everything in
[OPTIMIZATIONS.md](OPTIMIZATIONS.md) happens to every function, every time.
What is left to choose is in the source. This is what acc does well and
what it does badly, and how to write a hot loop so that it gets the first.

The code shown is what acc emits, and the timings are from an emulated
Agon at 18.432 MHz. A program written this way still builds with agondev.

## 1. Keep the hottest local in IY with `register`

A local or a parameter declared `register` lives in IY instead of in the
frame. Every read of it is then a register move instead of a load from
`(ix+d)`, adding a small constant to it changes nothing but a displacement,
and memory through it is reached directly:

| C, with `register char *p` | code |
| --- | --- |
| `c = *p;` | `ld a,(iy+0)` |
| `c = p[3];` | `ld a,(iy+3)` |
| `p++;` | `inc iy` |
| `*p++ = c;` | `inc iy`, and the store to `(iy-1)` |
| `p += 20;` | `lea iy,iy+20` |

The rules:

- **One per function.** The first eligible local or parameter declared
  `register` gets IY; if a parameter is one, it comes first. Any others are
  ordinary locals: `register` is otherwise accepted and ignored.
- **An `int` or a pointer**: three bytes, a number. A `char`, `short`,
  `long`, `float` or struct declared `register` stays in the frame.
- **Its address cannot be taken.** C forbids `&` on a register variable,
  and acc refuses it: `'x' is declared register, so it has no address to
  take`. A pointer walked by a helper that takes `char **` cannot be the
  one.

What it costs:

- **Every call** in the function pushes IY before it and pops it after,
  four bytes a call site, since the callee may use IY.
- **acc's own uses of IY** in that function save it and put it back the
  same way. Those are copies and constants of `long`, `long long` and
  `float`; an AND with a mask like `0x8000` or `0xffff`; a conversion to
  `short`; and a frame slot past `(ix-128)`.

So the best candidate is the pointer or counter a loop uses on every turn,
in a function whose loop calls nothing, or little, and does no `long` or
`float` work. A function that recurses, or whose loop is mostly calls, can
lose more at the calls than it gains between them.

**Measure it.** zap, the assembler, had 81 candidates, found by counting
uses in loops. Taken one function at a time while zap assembles BBC BASIC:

- 50 were worth keeping, saving cycles at no cost in bytes, or saving
  more than 1,000 cycles for each byte they added.
- Its recursive expression parser, a directive dispatcher and a table
  builder ran slower.
- Some added bytes to functions that the source never runs.

Annotating 78 of them made zap 3.6% faster and 182 bytes bigger. The 50
made it 3.8% faster and 981 bytes smaller.

agondev ignores `register`: zap compiles to the same assembly there with
or without it.

## 2. Divide in the narrowest width the value fits

`int` is 24 bits here, up to 8,388,607. A divide by a power of two up to
256 is a shift (section 5): `x / 4` takes 59 cycles, and an unsigned
`u / 4` 41. An unsigned `u % 8` is a mask, 6 cycles. Any other divide is
a call to the divide routine, about 500 to 600 cycles for an `int`.
An `int` divide works in registers. A `long` or
`long long` one takes its operands through frame slots and does more work
at every step, so the same division costs more the wider its type.

So divide in `int` whenever the values fit. acc's own `printf` used to
widen every integer to `unsigned long long` and divide it twice per
digit. Dividing by bytes from the top while the value is wider than 24
bits, and in `int` after that, made integer `sprintf` 26% faster. The
same change in `printf`'s float code, whose numbers were provably under
655,360, made it 18% faster.

## 3. Prefer unsigned

An unsigned comparison is a subtract and one jump on carry. A signed
one is turned into an unsigned one by adding `0x800000` to both sides,
six more bytes. acc skips that when neither side can be negative, and
against a constant it folds the adjustment in at compile time, but two
signed variables pay it at every compare.

Counters, sizes and indexes that are never negative should be `unsigned`.
Nothing else gets dearer for it, and an unsigned divide is cheaper too.

## 4. Work on bytes in `unsigned char`, and never in `short`

When an expression of `+ - & | ^ <<` goes straight into a `char`, acc
does it in A, a byte at a time, and never widens. `c = a + b;` with three
`unsigned char` locals is three instructions. A byte right shift by a
constant is that many `srl a` (or `sra a` for signed).

`short` is the eZ80's worst width. The same six operations measured 207
bytes as `unsigned char`, 277 as `unsigned int` and 406 as `unsigned
short`. Use `short` only for data whose layout needs two bytes, and do the
arithmetic in `int`.

## 5. Shifts

| shift | code |
| --- | --- |
| `x << n`, constant `n` up to 8 | `add hl,hl` n times |
| `x >> n`, constant `n` from 1 to 8 | a call to a routine with no loop: 38 to 52 cycles |
| `x >> n`, constant `n` from 16 to 23 | HL's top byte through the stack, 8 bytes, then a shift per bit past 16 |
| a byte by a constant, into a byte | `sla a`, `srl a` or `sra a` n times |
| any other `int` shift | a call to the shift helper, which loops: about 25 cycles a bit |

A right shift by 1 to 8 is `xor a`, or four bytes that put the sign in A
for a signed value, and a call: 5 or 8 bytes. `x >> 8` takes 38 cycles.
If `x` is a local or in memory, reading the byte is cheaper still:
`((unsigned char *)&x)[1]` is a `lea`, an `inc` and a load. Avoid shifts
by 9 to 15, which still loop.

## 6. Multiplies and strides

A multiply by a constant up to 65,535 is shifts and adds when that takes
12 steps or fewer, which covers the usual struct strides: indexing an
array of a 13-byte struct is 9 bytes of `add hl,hl` and `add hl,de` instead of a
call, and runs in about a thirtieth of the time. A multiply of two
variables is a call.

In a hot loop, walking a pointer is still cheaper than indexing. `p++` is
`inc hl`s for a stride up to 4, and `inc iy`s or one `lea` if `p` is the
register local.

## 7. Calls, and what gets inlined

A call costs about 60 cycles before the callee does anything. acc expands
a `static inline` function in place only if its body is a single `return`
of an int-or-narrower value, with at most eight int-or-narrower
parameters and no varargs. Any other `inline` function is called.

So a character-class test or an accessor used in a hot loop should be
written as one `return` expression:

```c
static inline int is_digit(int c) { return (unsigned) (c - '0') < 10; }
```

A `static` function nothing calls, and a `static inline` one nothing
uses, is dropped from the object. A header full of small helpers costs a
file only the ones it uses.

## 8. `memcpy`, `memset`, `memmove`, `memchr`

These four are built in: calls to routines built on `ldir` and `cpir`,
with the operands passed in registers. A byte loop written in C cost zap
7% of its running time. Struct assignment is an `ldir` too.

## 9. `switch` is a chain of compares

There are no jump tables. The value is loaded once, and each `case` is
tested in the order it appears in the source: twelve bytes each, and
one more compare per case before the one that matches. Put the most
frequent cases first. For a dense dispatch over many values, index a
table of function pointers or of data instead.

## 10. `long`, `long long` and `float`

These live in frame slots, and almost every operation on them is a call:
`lea hl,ix+a; lea de,ix+b; call`. `&`, `|`, `^` with a constant, and
shifts by 1, 8, 16 or 24, are done on the slot's bytes directly. Everything
else costs a call. If an `int` holds the value, use it.

The float routines, in cycles per call with the code around it:

| operation | cycles |
| --- | --- |
| `+` | 381 |
| `-` | 417 |
| `*` | 555 |
| `/` | 946 |
| `<` and the other compares | about 200 |
| `int` to `float` | 153 |
| `float` to `int` | 296 |
| `long` to `float` | 271 |
| `float` to `long` | 407 |

A value computed twice costs its calls twice. Keep a repeated product in a
variable: the Mandelbrot loop in test/perf's fp.c spends a tenth of its time
on the `zr * zr` it computes for the test and again for the body.

## 11. Keep the frame small

`(ix+d)` reaches 128 bytes below the frame pointer. acc keeps locals in
the first 96, but a function whose locals and scratch pass 128 reaches
the rest through IY at several bytes an access. A large array or struct
used by one function at a time belongs in a `static`.

## 12. Smaller things

- `i++;` as a statement is a load, an `inc` and a store. Used as a
  value, `a[i++]`, it also steps back after the store.
- A comparison in an `if`, a loop condition, `!`, `&&` or `||` is a jump
  on the flags. Storing one, `ok = a < b;`, builds the 0 or 1: `ld hl,1`,
  a jump and `ld hl,0`, ten bytes more.
- A function returning `char` or `unsigned char` returns it in A, and
  `if (f())` tests A directly.
