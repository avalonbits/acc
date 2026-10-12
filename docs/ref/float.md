# `float.h`

The characteristics of the floating types, as macros. `float` and
`double` are the same type on acc, IEEE 754 single precision in four
bytes, so every `DBL_` macro has the value of the `FLT_` one. They say
what the machine does rather than what C99 asks of a `double`: C99
wants `DBL_DIG` to be at least 10 and `DBL_EPSILON` at most 1e-9, and a
four-byte float gives 6 and about 1.2e-7. A program that needs that
precision does not have it here.

The `LDBL_` macros describe agondev's `long double`, an IEEE 754 double
in eight bytes. acc refuses `long double`, and so it refuses those of
them that are `long double` constants (`LDBL_MAX`, `LDBL_EPSILON`,
`LDBL_MIN`); the integer ones can be read.

## `FLT_RADIX`, `FLT_ROUNDS`, `FLT_EVAL_METHOD`, `DECIMAL_DIG`

    #define FLT_RADIX        2
    #define FLT_ROUNDS       1
    #define FLT_EVAL_METHOD  0
    #define DECIMAL_DIG      17

`FLT_RADIX` is 2: the numbers are binary. `FLT_ROUNDS` is 1, to nearest
with ties to even, which is what the runtime's arithmetic and the
compiler's folding of constants both do, and the only way; `fenv.h`
cannot change it. `FLT_EVAL_METHOD` is 0: every operation is carried
out in its own type, which here is always the four bytes, so
`float_t` and `double_t` in `math.h` are `float` and `double`.

`DECIMAL_DIG` is the `long double`'s, it being the widest floating type
C knows of here.

## `FLT_MANT_DIG`, `FLT_DIG`, `FLT_MIN_EXP`, `FLT_MIN_10_EXP`, `FLT_MAX_EXP`, `FLT_MAX_10_EXP`, `FLT_MAX`, `FLT_EPSILON`, `FLT_MIN`

IEEE 754 single precision. `FLT_MAX`, `FLT_EPSILON` and `FLT_MIN` are
`float` constants (with an `F`).

| Macro            | Value          | What it is                        |
|------------------|----------------|-----------------------------------|
| `FLT_MANT_DIG`   | 24             | bits of significand               |
| `FLT_DIG`        | 6              | decimal digits a float keeps      |
| `FLT_MIN_EXP`    | -125           | least e with 2^(e-1) normal       |
| `FLT_MIN_10_EXP` | -37            | least power of ten that is normal |
| `FLT_MAX_EXP`    | 128            | greatest e with 2^(e-1) finite    |
| `FLT_MAX_10_EXP` | 38             | greatest power of ten that fits   |
| `FLT_MAX`        | 3.40282347e+38 | the largest finite float          |
| `FLT_EPSILON`    | 1.19209290e-7  | 1 to the next float above it      |
| `FLT_MIN`        | 1.17549435e-38 | the smallest normal float         |

Below `FLT_MIN` are the subnormals, down to about 1.4e-45, which the
arithmetic and the library both handle; there is no `FLT_TRUE_MIN`,
that being C11's.

## `DBL_MANT_DIG`, `DBL_DIG`, `DBL_MIN_EXP`, `DBL_MIN_10_EXP`, `DBL_MAX_EXP`, `DBL_MAX_10_EXP`, `DBL_MAX`, `DBL_EPSILON`, `DBL_MIN`

The `FLT_` values, `double` being the same type as `float`; `DBL_MAX`,
`DBL_EPSILON` and `DBL_MIN` are the same numbers as `double` constants.

| Macro            | Value          | What it is                        |
|------------------|----------------|-----------------------------------|
| `DBL_MANT_DIG`   | 24             | bits of significand               |
| `DBL_DIG`        | 6              | decimal digits a double keeps     |
| `DBL_MIN_EXP`    | -125           | least e with 2^(e-1) normal       |
| `DBL_MIN_10_EXP` | -37            | least power of ten that is normal |
| `DBL_MAX_EXP`    | 128            | greatest e with 2^(e-1) finite    |
| `DBL_MAX_10_EXP` | 38             | greatest power of ten that fits   |
| `DBL_MAX`        | 3.40282347e+38 | the largest finite double         |
| `DBL_EPSILON`    | 1.19209290e-7  | 1 to the next double above it     |
| `DBL_MIN`        | 1.17549435e-38 | the smallest normal double        |

## `LDBL_MANT_DIG`, `LDBL_DIG`, `LDBL_MIN_EXP`, `LDBL_MIN_10_EXP`, `LDBL_MAX_EXP`, `LDBL_MAX_10_EXP`, `LDBL_MAX`, `LDBL_EPSILON`, `LDBL_MIN`

agondev's eight-byte `long double`, given as agondev gives it. Only the
integers are of use with acc; the last three are `long double`
constants, and draw the same refusal as the type.

| Macro             | Value                     |
|-------------------|---------------------------|
| `LDBL_MANT_DIG`   | 53                        |
| `LDBL_DIG`        | 15                        |
| `LDBL_MIN_EXP`    | -1021                     |
| `LDBL_MIN_10_EXP` | -307                      |
| `LDBL_MAX_EXP`    | 1024                      |
| `LDBL_MAX_10_EXP` | 308                       |
| `LDBL_MAX`        | 1.7976931348623157e+308L  |
| `LDBL_EPSILON`    | 2.2204460492503131e-16L   |
| `LDBL_MIN`        | 2.2250738585072014e-308L  |
