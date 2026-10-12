# `stddef.h`

The common definitions: the types of a size, of a difference between
pointers and of a wide character, the null pointer, and `offsetof`.

An eZ80 in ADL mode addresses three bytes, and acc's `int` is three
bytes, so a size and a pointer difference are both as wide as an `int`:
24 bits.

## `size_t`

    typedef unsigned int size_t;

The type of a size: what `sizeof` answers and what `malloc`, `strlen`
and the other functions of sizes take and answer. It is an `unsigned
int`, 24 bits, from 0 to 16777215 (`SIZE_MAX` in
[`stdint.h`](stdint.md)). `printf` and `scanf` read one with `%zu`, or
with a plain `%u`, since it is an `unsigned int` already.

`stdio.h`, `stdlib.h`, `string.h` and other headers give `size_t` too,
the same type.

## `ptrdiff_t`

    typedef int ptrdiff_t;

The type of one pointer less another. It is an `int`, 24 bits, from
-8388608 to 8388607 (`PTRDIFF_MIN` and `PTRDIFF_MAX` in
[`stdint.h`](stdint.md)). `%td` reads one, as does a plain `%d`.

## `wchar_t`

    typedef short wchar_t;

A wide character, as agondev has it: 16 bits, signed. It is what `L'x'`
is and what an `L"..."` string is an array of. Its limits, `WCHAR_MIN`
of -32768 and `WCHAR_MAX` of 32767, are in [`stdint.h`](stdint.md) and
`wchar.h`.

Sixteen bits holds the characters UTF-8 writes in one, two or three
bytes, up to U+FFFF, and no further; a character past that has no
`wchar_t`.

## `NULL`

    #define NULL ((void *) 0)

The null pointer constant. `stdio.h`, `stdlib.h`, `string.h`,
[`stdint.h`](stdint.md) and others define it the same way, each only
when it is not defined yet.

## `offsetof`

    #define offsetof(type, member) __builtin_offsetof(type, member)

How many bytes into the struct or union `type` its `member` starts, as
a constant: one that can size an array or be a `case`.

The compiler works it out. The usual definition,
`&((type *) 0)->member`, takes the address of something through a null
pointer, which acc refuses everywhere else as the mistake it is.

The member may be reached through others and through subscripts:

    struct line { int number; char text[80]; };
    struct page { int count; struct line lines[24]; };

    offsetof(struct page, lines[2].text)    /* 3 + 2 * 83 + 3 = 172 */

acc puts no padding between members, as agondev does not: every member
starts where the one before it ends.

    struct s { char c; long l; };

    offsetof(struct s, l)                   /* 1 */

A `type` that is not a struct or a union is an error.
