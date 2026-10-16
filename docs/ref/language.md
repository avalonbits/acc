# The language

acc compiles C99, at agondev's widths, so that a program means the same
built by either. What it leaves out, and how far it goes, is below.

## Types, `sizeof`

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

## What C99 is there

Every operator and statement; declarations anywhere in a block; functions,
prototypes, pointers to functions and variable arguments (`stdarg.h`);
`struct`, `union`, `enum`, `typedef`, bit-fields and flexible array
members; arrays of any dimension, variable-length ones included;
designated initialisers and compound literals; structs passed, returned
and assigned by value; `static`, `extern`, `const`, `volatile`,
`register`, `auto`, `inline` and `restrict`; `__func__`; `//` comments;
and the whole preprocessor.

## `long double`, `_Complex`

`long double` is refused: agondev's library has no arithmetic for one, so
a program that used it could not be linked by either. `_Complex` is
refused too. The `l`-suffixed functions of `math.h` are not there.

## Old-style definitions

A function defined the old way, with its parameters' types after the
parentheses, is refused: `int f(a) int a; { ... }`. So is a call with
arguments to a function defined with an empty `()`.

## `__attribute__`, GNU C

`__attribute__((...))` is read and ignored wherever it is written. acc
says it is GNU C 4.2.1 (`__GNUC__` is 4), as agondev's clang does, so a
program that asks before writing an attribute gets to write it.

Of GNU C's extensions, acc has `__builtin_offsetof` (what `offsetof`
is), `__builtin_unreachable`, `__builtin_ffs` and `__builtin_popcount`.
Statement expressions `({ })`, `typeof`, case ranges `case 1 ... 3`,
labels as values `&&label`, zero-length arrays, binary constants `0b101`
and `asm` are not: each is an error.

## `#pragma`

`#pragma once` reads the file once however often it is included.
`#pragma weak name` makes a reference to `name` no reason to link it: if
nothing else wants it, its address is 0. Any other pragma is ignored, as
C says.

## `__STDC_VERSION__`, predefined macros

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

## Limits

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
175 KB of heap for one source, which a file of about 2,000 small
functions, 2,000 macros or 3,000 globals fills, or one function of about
1,000 statements. A program too big for one file is split into several,
each compiled with `-c`, and the objects linked; one file compiled with
`-c` and linked alone gains nothing, since the link holds all of the
object at once. acc stops with `out of memory for ...` when it runs out,
says what it was making, and leaves no program or object behind.

A program's own stack is the 16 KB between its heap and the top of
memory, and nothing stops it running into the heap. Keep large arrays
`static` rather than local.
