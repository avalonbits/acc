# `stdint.h`

Integer types of exact widths, and the limits of each. The header has
all of C99's, and a 24-bit pair besides: 24 bits is the eZ80's own
width, that of an `int`, a register and an address.

| Type        | Bytes | Bits | Signed |
|-------------|-------|------|--------|
| `char`      | 1     | 8    | yes    |
| `short`     | 2     | 16   | yes    |
| `int`       | 3     | 24   | yes    |
| `long`      | 4     | 32   | yes    |
| `long long` | 8     | 64   | yes    |

Wherever C leaves a choice open (which type is the fastest of a width,
which is the smallest that holds one), acc makes agondev's, so a struct
of these types has the same layout whichever of the two compiled it.

The header also defines `NULL`, as `stddef.h` does, since agondev's
`stdint.h` has it too.

## `int8_t`, `int16_t`, `int24_t`, `int32_t`, `int64_t`

    typedef signed char        int8_t;
    typedef short              int16_t;
    typedef int                int24_t;
    typedef long               int32_t;
    typedef long long          int64_t;

The signed types of exact widths, and their limits:

| Type      | Is            | Least (`INTn_MIN`)   | Most (`INTn_MAX`)   |
|-----------|---------------|----------------------|---------------------|
| `int8_t`  | `signed char` | -128                 | 127                 |
| `int16_t` | `short`       | -32768               | 32767               |
| `int24_t` | `int`         | -8388608             | 8388607             |
| `int32_t` | `long`        | -2147483648          | 2147483647          |
| `int64_t` | `long long`   | -9223372036854775808 | 9223372036854775807 |

`int24_t` is not C99's; agondev has it.

## `uint8_t`, `uint16_t`, `uint24_t`, `uint32_t`, `uint64_t`

    typedef unsigned char      uint8_t;
    typedef unsigned short     uint16_t;
    typedef unsigned int       uint24_t;
    typedef unsigned long      uint32_t;
    typedef unsigned long long uint64_t;

The unsigned types of exact widths. Each goes from 0 to its most:

| Type       | Is                   | Most (`UINTn_MAX`)       |
|------------|----------------------|--------------------------|
| `uint8_t`  | `unsigned char`      | 255                      |
| `uint16_t` | `unsigned short`     | 65535                    |
| `uint24_t` | `unsigned int`       | 16777215                 |
| `uint32_t` | `unsigned long`      | 4294967295               |
| `uint64_t` | `unsigned long long` | 18446744073709551615     |

`uint24_t` is not C99's; agondev has it.

## `INT8_MAX`, `INT16_MAX`, `INT24_MAX`, `INT32_MAX`, `INT64_MAX`

    #define INT8_MAX     127
    #define INT16_MAX    32767
    #define INT24_MAX    8388607
    #define INT32_MAX    2147483647L
    #define INT64_MAX    9223372036854775807LL

The most each signed type of an exact width holds. Each is written with
the type a value of that width promotes to: an `int` up to 24 bits, then
a `long` and a `long long`.

## `INT8_MIN`, `INT16_MIN`, `INT24_MIN`, `INT32_MIN`, `INT64_MIN`

    #define INT8_MIN     (-128)
    #define INT16_MIN    (-32768)
    #define INT24_MIN    (-8388608)
    #define INT32_MIN    (-2147483647L - 1L)
    #define INT64_MIN    (-9223372036854775807LL - 1LL)

The least each signed type of an exact width holds. The 32- and 64-bit
ones are written as a subtraction, since the positive half of the
number is past the type's range.

## `UINT8_MAX`, `UINT16_MAX`, `UINT24_MAX`, `UINT32_MAX`, `UINT64_MAX`

    #define UINT8_MAX    255
    #define UINT16_MAX   65535
    #define UINT24_MAX   16777215u
    #define UINT32_MAX   4294967295UL
    #define UINT64_MAX   18446744073709551615ULL

The most each unsigned type of an exact width holds. `UINT8_MAX` and
`UINT16_MAX` are plain `int` constants, since a `uint8_t` or `uint16_t`
promotes to an `int`.

## `int_least8_t`, `int_fast8_t`, `uint_least8_t`, `uint_fast8_t`

    typedef signed char        int_least8_t;    /* and int_fast8_t */
    typedef short              int_least16_t;   /* and int_fast16_t */
    typedef long               int_least32_t;   /* and int_fast32_t */
    typedef long long          int_least64_t;   /* and int_fast64_t */

The smallest type that holds a width, and the fastest. For each of 8,
16, 32 and 64 bits, both are the exact type of that width:
`int_least16_t` and `int_fast16_t` are `int16_t`, and `uint_least32_t`
and `uint_fast32_t` are `uint32_t`. There is no 24-bit one of either
kind.

Their limits are the exact type's: `INT_LEAST16_MAX` and
`INT_FAST16_MAX` are `INT16_MAX`, `UINT_FAST8_MAX` is `UINT8_MAX`, and so
on, for `INT_LEASTn_MIN`, `INT_LEASTn_MAX`, `UINT_LEASTn_MAX`,
`INT_FASTn_MIN`, `INT_FASTn_MAX` and `UINT_FASTn_MAX`.

## `intptr_t`, `uintptr_t`, `INTPTR_MAX`, `UINTPTR_MAX`

    typedef int                intptr_t;
    typedef unsigned int       uintptr_t;

Integers that hold a pointer: an address is three bytes, and so is an
`int`. `INTPTR_MIN` and `INTPTR_MAX` are -8388608 and 8388607, and
`UINTPTR_MAX` is 16777215.

## `intmax_t`, `uintmax_t`, `INTMAX_MAX`, `UINTMAX_MAX`

    typedef long long          intmax_t;
    typedef unsigned long long uintmax_t;

The widest integers: 64 bits. `INTMAX_MIN`, `INTMAX_MAX` and
`UINTMAX_MAX` are `INT64_MIN`, `INT64_MAX` and `UINT64_MAX`.
[`inttypes.h`](16-inttypes.md) has the functions over them.

## `SIZE_MAX`, `PTRDIFF_MIN`, `PTRDIFF_MAX`, `WCHAR_MIN`, `WCHAR_MAX`

The limits of the integer types other headers define:

| Macro            | Value      | The type, and where it is          |
|------------------|------------|------------------------------------|
| `SIZE_MAX`       | 16777215u  | `size_t`, `unsigned int`; stddef.h |
| `PTRDIFF_MIN`    | -8388608   | `ptrdiff_t`, `int`; stddef.h       |
| `PTRDIFF_MAX`    | 8388607    |                                    |
| `SIG_ATOMIC_MIN` | -8388608   | `sig_atomic_t`, `int`; signal.h    |
| `SIG_ATOMIC_MAX` | 8388607    |                                    |
| `WCHAR_MIN`      | -32768     | `wchar_t`, `short`; stddef.h       |
| `WCHAR_MAX`      | 32767      |                                    |
| `WINT_MIN`       | -8388608   | `wint_t`, `int`; wchar.h           |
| `WINT_MAX`       | 8388607    |                                    |

`wchar.h` defines `WCHAR_MIN` and `WCHAR_MAX` too, the same, and
whichever header comes second leaves them as the first defined them.

## `INT32_C`, `UINT32_C`, `INT64_C`, `UINT64_C`, `INTMAX_C`, `UINTMAX_C`

    #define INT32_C(c)   c ## L
    #define UINT64_C(c)  c ## ULL

A constant written with the suffix of the least type of a width, as it
is once promoted:

| Macro                    | Suffix | Example                           |
|--------------------------|--------|-----------------------------------|
| `INT8_C`, `INT16_C`      | none   | `INT16_C(1000)` is `1000`         |
| `UINT8_C`, `UINT16_C`    | none   | `UINT8_C(255)` is `255`           |
| `INT32_C`                | `L`    | `INT32_C(1)` is `1L`              |
| `UINT32_C`               | `UL`   | `UINT32_C(1)` is `1UL`            |
| `INT64_C`, `INTMAX_C`    | `LL`   | `INT64_C(1)` is `1LL`             |
| `UINT64_C`, `UINTMAX_C`  | `ULL`  | `UINTMAX_C(1)` is `1ULL`          |

Eight and sixteen bits need no suffix, since either is an `int` once
promoted. There is no `INT24_C` or `UINT24_C`: a constant with no suffix
is already an `int` when it fits one.

    uint32_t top = UINT32_C(1) << 31;   /* 1 << 31 is past an int */
