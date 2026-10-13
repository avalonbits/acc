# `limits.h`

The ranges of the integer types.

An `int` here is 24 bits, the width of a register and of an address, so
`INT_MAX` is 8388607 and not the 2147483647 a program written elsewhere
expects. A `long` is 32 bits and a `long long` 64, as agondev has them.
`char` is signed, as it is on this chip.

## `CHAR_BIT`

    #define CHAR_BIT 8

The bits in a byte, and so in a `char`.

## `CHAR_MIN`, `CHAR_MAX`, `SCHAR_MIN`, `SCHAR_MAX`, `UCHAR_MAX`

| Macro       | Value | Type            |
|-------------|-------|-----------------|
| `SCHAR_MIN` | -128  | `signed char`   |
| `SCHAR_MAX` | 127   | `signed char`   |
| `UCHAR_MAX` | 255   | `unsigned char` |
| `CHAR_MIN`  | -128  | `char`          |
| `CHAR_MAX`  | 127   | `char`          |

`char` is signed, so `CHAR_MIN` and `CHAR_MAX` are defined as
`SCHAR_MIN` and `SCHAR_MAX`.

## `SHRT_MIN`, `SHRT_MAX`, `USHRT_MAX`

| Macro       | Value  | Type             |
|-------------|--------|------------------|
| `SHRT_MIN`  | -32768 | `short`          |
| `SHRT_MAX`  | 32767  | `short`          |
| `USHRT_MAX` | 65535  | `unsigned short` |

## `INT_MIN`, `INT_MAX`, `UINT_MAX`

| Macro      | Value     | Written as   |
|------------|-----------|--------------|
| `INT_MIN`  | -8388608  | `(-8388608)` |
| `INT_MAX`  | 8388607   | `8388607`    |
| `UINT_MAX` | 16777215  | `16777215u`  |

An `int` is 24 bits. A program that assumes 32 (that `4096 * 4096` is
16777216 and not 0, or that an `int` holds the milliseconds in a day)
needs a `long`.

## `LONG_MIN`, `LONG_MAX`, `ULONG_MAX`

| Macro       | Value       | Written as             |
|-------------|-------------|------------------------|
| `LONG_MIN`  | -2147483648 | `(-2147483647L - 1L)`  |
| `LONG_MAX`  | 2147483647  | `2147483647L`          |
| `ULONG_MAX` | 4294967295  | `4294967295UL`         |

## `LLONG_MIN`, `LLONG_MAX`, `ULLONG_MAX`

| Macro        | Value                | Written as                       |
|--------------|----------------------|----------------------------------|
| `LLONG_MIN`  | -9223372036854775808 | `(-9223372036854775807LL - 1LL)` |
| `LLONG_MAX`  | 9223372036854775807  | `9223372036854775807LL`          |
| `ULLONG_MAX` | 18446744073709551615 | `18446744073709551615ULL`        |

## `MB_LEN_MAX`

    #define MB_LEN_MAX 3

The most bytes a multibyte character takes. Multibyte characters are
UTF-8, as far as a 16-bit `wchar_t` goes: one to three bytes, up to
U+FFFF, and a four-byte sequence is not one. See
[`mblen`](stdlib.md#mblen-mbtowc-wctomb-mb_cur_max).
