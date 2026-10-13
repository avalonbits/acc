# Numbers

The mathematical functions, the floating-point environment, and the integer and floating types with their limits and formats.

## `math.h`

C99's mathematics, as far as a machine with one floating type can go.
`float` and `double` are the same four-byte IEEE 754 single precision
type on acc, and acc refuses `long double`, so each function is there
under its `double` name and its `float` name (`sin` and `sinf`), the two
being the same code, and no function of the `long double` family (`sinl`
and the rest) is there at all.

The header also gives the types `float_t` and `double_t`; the
classification macros; the comparisons that are quiet about a NaN; and
the constants `HUGE_VAL`, `HUGE_VALF`, `INFINITY` and `NAN`.

Twenty-four bits of significand is about seven decimal digits. The
functions that need not approximate (`fabs`, `floor`, `fmod`, `frexp`,
`sqrt`, `fma` and the like) answer exactly, or rounded once. Each of the
others was measured against the host's own library, and what was found
is given with it, in units in the last place (ulp).

There are no builtins: every one of these is a call into the library,
and only the functions a program calls are linked into it.

### `float_t`, `double_t`

    typedef float  float_t;
    typedef double double_t;

The types an expression of `float` and of `double` is worked out in.
Each is the type itself, since `FLT_EVAL_METHOD` is 0: every operation
is carried out in the type it is given. Both are four bytes.

### `math_errhandling`, `MATH_ERRNO`, `MATH_ERREXCEPT`

    #define MATH_ERRNO       1
    #define MATH_ERREXCEPT   2
    #define math_errhandling MATH_ERRNO

Errors are reported in `errno`, and only there: `fenv.h` has no
exception flags to raise.

- A **domain error**, an argument the function has no answer for (`sqrt`
  of a negative, `log` of a negative, `asin(2)`, `sin` of an infinity),
  sets `errno` to `EDOM` and answers a NaN.
- A **pole**, an exact infinity at a finite argument (`log(0)`,
  `atanh(1)`, `tgamma(0)`), sets `errno` to `ERANGE` and answers an
  infinity of the right sign.
- An **overflow**, a finite result too large for a float, sets `errno`
  to `ERANGE` and answers `HUGE_VAL` with the right sign.
- A result **too small** is not an error: the answer is the nearest
  number there is, a subnormal or zero, and `errno` is left alone.

A function never clears `errno`, so a program that wants to know sets it
to 0 first.

    errno = 0;
    y = log(x);
    if (errno == EDOM)
        puts("no logarithm");

A NaN argument answers a NaN, with no error, unless its function's
section says otherwise.

### `HUGE_VAL`, `HUGE_VALF`, `INFINITY`, `NAN`

    #define HUGE_VAL   1e39
    #define HUGE_VALF  1e39f
    #define INFINITY   1e39f
    #define NAN        (0.0f / 0.0f)

`HUGE_VAL`, `HUGE_VALF` and `INFINITY` are positive infinity: a float
literal past the largest float is an infinity to acc, as Annex F has it.
`NAN` is the quiet NaN (bits `0x7fc00000`), which the division of zero by
zero gives. All four are constants, folded by the compiler, so a static
may be initialised to them.

The runtime's arithmetic carries both values as IEEE 754 does: a finite
number over zero is an infinity, zero over zero and an infinity less
itself are NaNs, an overflow is an infinity, a NaN in any operation
makes a NaN, and a comparison with a NaN on either side is false (and
`!=` true). Converting an infinity or a NaN to an integer is undefined.

There is no `HUGE_VALL`, there being no `long double`.

### `fpclassify`, `isnan`, `isinf`, `isfinite`, `isnormal`, `signbit`, `FP_NAN`, `FP_INFINITE`, `FP_ZERO`, `FP_SUBNORMAL`, `FP_NORMAL`

    #define FP_NAN       0
    #define FP_INFINITE  1
    #define FP_ZERO      2
    #define FP_SUBNORMAL 3
    #define FP_NORMAL    4

    int fpclassify(x);
    int isnan(x);
    int isinf(x);
    int isfinite(x);
    int isnormal(x);
    int signbit(x);

Macros, which read the bits of `x`. `fpclassify` answers which of the
five kinds of number `x` is; `isnan`, `isinf`, `isfinite` (neither of
those two) and `isnormal` answer 1 or 0. `signbit` answers 1 when the
sign bit is set, which it is for `-0.0` and for a negative NaN, and 0
otherwise.

Each is a call to a function (`__acc_fpclassify`, `__acc_signbit`), so
`x` is evaluated once and may be any expression, and is converted to
`double`, which here changes nothing.

### `isgreater`, `isgreaterequal`, `isless`, `islessequal`, `islessgreater`, `isunordered`

    int isgreater(x, y);
    int isgreaterequal(x, y);
    int isless(x, y);
    int islessequal(x, y);
    int islessgreater(x, y);
    int isunordered(x, y);

Macros that compare two numbers, each a call to a function so that each
argument is evaluated once. The first five are `>`, `>=`, `<`, `<=`, and
`<` or `>`, which are already false when either side is a NaN.
`isunordered` answers 1 when either is a NaN.

### `fabs`, `fabsf`, `copysign`, `copysignf`

    double fabs(double x);
    double copysign(double x, double y);
    float  fabsf(float x);
    float  copysignf(float x, float y);

`fabs` is `x` with its sign bit cleared, and `copysign` is `x`'s
magnitude with `y`'s sign bit. Both work on the bits, so they are exact
for every value, and keep a NaN a NaN: `fabs(-0.0)` is `+0.0`, and
`copysign(1, -0.0)` is -1.

### `floor`, `ceil`, `trunc`, `floorf`, `ceilf`, `truncf`

    double floor(double x);
    double ceil(double x);
    double trunc(double x);
    float  floorf(float x);
    float  ceilf(float x);
    float  truncf(float x);

The whole number below `x` (`floor`), above it (`ceil`), and toward zero
(`trunc`). Exact for every value. A number of 2^23 or more is whole
already and is answered as it is, as are infinities and NaNs. An answer
of zero keeps `x`'s sign: `trunc(-0.5)` and `ceil(-0.5)` are `-0.0`.

### `round`, `roundf`, `lround`, `lroundf`, `llround`, `llroundf`

    double    round(double x);
    float     roundf(float x);
    long      lround(double x);
    long      lroundf(float x);
    long long llround(double x);
    long long llroundf(float x);

The nearest whole number, a half going away from zero: `round(2.5)` is
3 and `round(-2.5)` is -3. Exact.

`lround` and `llround` answer it as a `long` or a `long long`. One too
large for the type is not checked, and no error is set: the conversion
wraps.

### `rint`, `nearbyint`, `rintf`, `nearbyintf`, `lrint`, `lrintf`, `llrint`, `llrintf`

    double    rint(double x);
    double    nearbyint(double x);
    float     rintf(float x);
    float     nearbyintf(float x);
    long      lrint(double x);
    long      lrintf(float x);
    long long llrint(double x);
    long long llrintf(float x);

The nearest whole number, a half going to the even one: `rint(2.5)` is
2 and `rint(3.5)` is 4. That is the rounding every operation on acc
does, and there is no other mode, so `nearbyint` is `rint`. Exact.
`rint(-0.5)` is `-0.0`.

`lrint` and `llrint` answer it as a `long` or a `long long`, and as
`lround` does, a value too large for the type wraps, with no error.

### `fmod`, `fmodf`

    double fmod(double x, double y);
    float  fmodf(float x, float y);

What is left of `x` when whole multiples of `y` are taken from it, with
`x`'s sign: `fmod(7, 3)` is 1 and `fmod(-7, 3)` is -1. Exact for every
value; each subtraction it makes loses nothing.

A `y` of zero, or an infinite `x`, is a domain error (`EDOM`, a NaN). An
infinite `y`, or an `x` smaller than `y`, answers `x`.

### `remainder`, `remquo`, `remainderf`, `remquof`

    double remainder(double x, double y);
    double remquo(double x, double y, int *quo);
    float  remainderf(float x, float y);
    float  remquof(float x, float y, int *quo);

`x` less the multiple of `y` nearest to it, a tie going to the even
multiple, so the answer is at most half of `y` either way:
`remainder(5, 2)` is 1 and `remainder(7, 2)` is -1. Exact.

`remquo` also stores in `*quo` the last three bits of that multiple
(0 to 7), negative when `x / y` is.

A `y` of zero, or an infinite `x`, is a domain error (`EDOM`, a NaN, and
`*quo` 0). An infinite `y` answers `x`.

### `modf`, `modff`

    double modf(double x, double *ip);
    float  modff(float x, float *ip);

Split `x` into its whole part, stored in `*ip`, and its fraction, which
is the answer. Both have `x`'s sign: `modf(-3.5, &w)` answers -0.5 and
stores -3, and `modf(-3.0, &w)` answers `-0.0`. An infinity stores
itself and answers a zero. Exact.

### `frexp`, `ldexp`, `frexpf`, `ldexpf`

    double frexp(double x, int *exp);
    double ldexp(double x, int exp);
    float  frexpf(float x, int *exp);
    float  ldexpf(float x, int exp);

`frexp` splits `x` into a fraction from 1/2 up to (not including) 1,
which it answers with `x`'s sign, and a power of two, which it stores in
`*exp`, so that `x` is the fraction times 2 to the `*exp`. A subnormal
is split as well as any other number. A zero, an infinity or a NaN is
answered as it is, with `*exp` set to 0.

`ldexp` is `x` times 2 to the `exp`, which undoes `frexp`; it is
`scalbn`. Both are exact, but for an `ldexp` that lands among the
subnormals, which is rounded once.

    int e;
    double f = frexp(48.0, &e);     /* 0.75, and e is 6 */
    double x = ldexp(f, e);         /* 48 */

### `scalbn`, `scalbln`, `scalbnf`, `scalblnf`

    double scalbn(double x, int n);
    double scalbln(double x, long n);
    float  scalbnf(float x, int n);
    float  scalblnf(float x, long n);

`x` times 2 to the `n`, worked out by multiplying, so a result among the
subnormals is rounded the way a multiplication rounds it. A finite `x`
that becomes an infinity is an overflow (`ERANGE`, an infinity of `x`'s
sign); a result too small is a subnormal or zero, with no error.

`scalbln` takes `n` as a `long`; one past 1000 either way answers as
1000 would.

### `ilogb`, `ilogbf`, `logb`, `logbf`, `FP_ILOGB0`, `FP_ILOGBNAN`

    #define FP_ILOGB0   (-8388607 - 1)
    #define FP_ILOGBNAN 8388607

    int    ilogb(double x);
    int    ilogbf(float x);
    double logb(double x);
    float  logbf(float x);

The power of two that `x`'s leading bit is worth, as an `int` (`ilogb`)
or as a floating value (`logb`): 1 for 2 and for 3.5, -1 for 0.5. A
subnormal's is its true exponent, below -126.

`ilogb` of zero answers `FP_ILOGB0` (`INT_MIN`), of a NaN `FP_ILOGBNAN`
(`INT_MAX`), and of an infinity `INT_MAX` too; each of the three is a
domain error, and sets `errno` to `EDOM`.

`logb` of zero answers minus infinity, of an infinity plus infinity, and
of a NaN the NaN; it sets no error for any of them.

### `fdim`, `fdimf`

    double fdim(double x, double y);
    float  fdimf(float x, float y);

`x - y` when `x` is the larger, and 0 otherwise. A NaN on either side
answers a NaN.

### `fmax`, `fmin`, `fmaxf`, `fminf`

    double fmax(double x, double y);
    double fmin(double x, double y);
    float  fmaxf(float x, float y);
    float  fminf(float x, float y);

The larger, or the smaller, of the two. A NaN on one side is ignored and
the other is the answer; with a NaN on both, the answer is a NaN.

### `nextafter`, `nextafterf`

    double nextafter(double x, double y);
    float  nextafterf(float x, float y);

The float next to `x` in the direction of `y`, one step of the bit
pattern: `nextafter(1, 2)` is `1 + FLT_EPSILON`. When `x` equals `y` the
answer is `y`; from a zero it is the smallest subnormal, with `y`'s
sign. A NaN on either side answers a NaN. A step past the largest float
is an overflow (`ERANGE`, an infinity).

There is no `nexttoward`, whose second argument is a `long double`.

### `nan`, `nanf`

    double nan(const char *tag);
    float  nanf(const char *tag);

The quiet NaN, bits `0x7fc00000`. `tag` is not read: every NaN these
make is the same one.

### `fma`, `fmaf`

    double fma(double x, double y, double z);
    float  fmaf(float x, float y, float z);

`x * y + z`, rounded once: the product is kept whole, in integers, and
only the sum is rounded, to nearest with ties to even. An answer that
overflows is an `ERANGE` infinity. When the product and `z` cancel
exactly the answer is `+0.0`.

A zero, an infinity or a NaN among the three is left to the ordinary
arithmetic, where `x * y` is then exact or a NaN.

It is worked out in 64-bit integers, so it is slower than `x * y + z`.

### `sqrt`, `sqrtf`

    double sqrt(double x);
    float  sqrtf(float x);

The square root, exact: the answer is the true root rounded to nearest,
ties to even. It was checked against the host's own square root for
every positive float, normal and subnormal.

It is worked out a bit at a time, a compare, a subtract and a shift per
bit, with no division. `sqrt(-0.0)` is `-0.0` and `sqrt(INFINITY)` is
an infinity. Any number below zero, minus infinity among them, is a
domain error (`EDOM`, a NaN).

### `cbrt`, `cbrtf`

    double cbrt(double x);
    float  cbrtf(float x);

The cube root, within one ulp over the whole range. A negative `x` has
a negative root, so it is never an error. A zero, an infinity or a NaN
is answered as it is.

### `hypot`, `hypotf`

    double hypot(double x, double y);
    float  hypotf(float x, float y);

The square root of `x * x + y * y`, within one ulp. Both are scaled
first, so the squares cannot overflow on the way to an answer that
does not: `hypot(3e30, 4e30)` is about `5e30`, though `3e30 * 3e30`
alone is an infinity. An answer too large is an overflow (`ERANGE`, an
infinity).

An infinity on either side answers an infinity, even with a NaN on the
other, as C99 F.9.4.3 has it; otherwise a NaN answers a NaN.

### `exp`, `exp2`, `expm1`, `expf`, `exp2f`, `expm1f`

    double exp(double x);
    double exp2(double x);
    double expm1(double x);
    float  expf(float x);
    float  exp2f(float x);
    float  expm1f(float x);

`exp` is e to the `x`, `exp2` is 2 to the `x`, and `expm1` is `exp(x) -
1`, worked out so that a small `x` keeps its digits: `expm1(1e-5)` is
right where `exp(1e-5) - 1` is out in the fourth digit.

Measured against the host's library over the whole range: `exp` and
`exp2` within one ulp, `expm1` within three.

`exp` of more than about 88.72, and `exp2` of 128 or more, is an
overflow (`ERANGE`, `HUGE_VAL`). Far enough below zero (`exp` below
about -103.97, `exp2` below -150) the answer is 0, with no error.
`exp(INFINITY)` is an infinity and `exp(-INFINITY)` is 0.

### `log`, `log2`, `log10`, `log1p`, `logf`, `log2f`, `log10f`, `log1pf`

    double log(double x);
    double log2(double x);
    double log10(double x);
    double log1p(double x);
    float  logf(float x);
    float  log2f(float x);
    float  log10f(float x);
    float  log1pf(float x);

The logarithm of `x` to base e, 2 and 10, and `log1p`, the natural
logarithm of `1 + x`, worked out so that a small `x` keeps its digits.

Measured against the host's library: `log` and `log2` within one ulp
over the whole range, `log10` within two, `log1p` within three.

Zero, of either sign, is a pole (`ERANGE`, minus infinity), and any
number below zero is a domain error (`EDOM`, a NaN). For `log1p` the
pole is at -1 and the domain error below it. `log(INFINITY)` is an
infinity.

`log2` and `log10` are `log` times a constant.

### `pow`, `powf`

    double pow(double x, double y);
    float  powf(float x, float y);

`x` to the power of `y`. Measured against the host's library over two
million random pairs: within two or three ulp for the exponents a
program is likely to ask for, and within fourteen at the extremes of
the range.

- `y` zero answers 1, and so does an `x` of 1, even where the other is a
  NaN. Otherwise a NaN on either side answers a NaN.
- A negative `x` has an answer only when `y` is a whole number, negative
  when `y` is odd; otherwise it is a domain error (`EDOM`, a NaN), as
  `pow(-8, 1.0/3)` is. `cbrt` is the cube root of a negative.
- `x` zero: a `y` above zero answers zero (`-0.0` for `-0.0` and an odd
  `y`). A `y` below zero is a pole (`ERANGE`, an infinity; negative for
  `-0.0` and an odd `y`).
- `x` infinite: a `y` above zero answers an infinity (negative for minus
  infinity and an odd `y`), and below zero answers 0 (`-0.0` for minus
  infinity and an odd `y`).
- An answer too large is an overflow (`ERANGE`, an infinity); one too
  small is zero, with no error.

- `y` infinite, C99 F.9.4.4's way: 1 for an `x` of 1 or -1; otherwise 0
  or an infinity, an infinity where `|x|` is below 1 and `y` is minus
  infinity or `|x|` is above 1 and `y` is infinity, with no error:
  `pow(0.5, INFINITY)` is 0 and `pow(0, -INFINITY)` an infinity.

### `sin`, `cos`, `tan`, `sinf`, `cosf`, `tanf`

    double sin(double x);
    double cos(double x);
    double tan(double x);
    float  sinf(float x);
    float  cosf(float x);
    float  tanf(float x);

The sine, cosine and tangent of `x` radians. `x` is brought down to
within pi/4 of a multiple of pi/2, with pi/2 taken away in five pieces
so that the remainder keeps its digits. `sin` and `tan` of `-0.0` are
`-0.0`.

Measured against the host's library: `sin` within two ulp over -100 to
100, and `cos` within eight, the eight being where the cosine passes
through zero. Past about 100000 the reduction runs out of digits, and
the answer degrades.

An infinity is a domain error (`EDOM`, a NaN).

### `asin`, `acos`, `atan`, `asinf`, `acosf`, `atanf`

    double asin(double x);
    double acos(double x);
    double atan(double x);
    float  asinf(float x);
    float  acosf(float x);
    float  atanf(float x);

The arc sine (from -pi/2 to pi/2), arc cosine (from 0 to pi) and arc
tangent (from -pi/2 to pi/2), in radians. Measured against the host's
library: `atan` and `asin` within two ulp, `acos` within one.
`atan(-0.0)` is `-0.0`.

`asin` and `acos` of a number past 1 either way are a domain error
(`EDOM`, a NaN). `atan` of an infinity is pi/2 with its sign.

### `atan2`, `atan2f`

    double atan2(double y, double x);
    float  atan2f(float y, float x);

The angle of the point (`x`, `y`) from the positive `x` axis, from -pi
to pi: `atan2(1, -1)` is 3pi/4. It is `atan(y / x)`, moved into the
right half of the circle by the signs.

When `x` is zero, the answer is pi/2 when `y` is above zero and -pi/2
otherwise. When both are zero, the signs of the zeros decide, as C99
has it: `atan2(0.0, -0.0)` is pi, `atan2(-0.0, -0.0)` is -pi, and with
`x` `+0.0` the answer is `y`'s zero. A negative `x` with `y` a zero
answers pi with `y`'s sign: `atan2(-0.0, -1)` is -pi.

A NaN on either side answers a NaN. Two infinities answer an odd
multiple of pi/4, by their signs: `atan2(INFINITY, -INFINITY)` is
3pi/4.

### `sinh`, `cosh`, `tanh`, `sinhf`, `coshf`, `tanhf`

    double sinh(double x);
    double cosh(double x);
    double tanh(double x);
    float  sinhf(float x);
    float  coshf(float x);
    float  tanhf(float x);

The hyperbolic sine, cosine and tangent, written so that a small `x`
keeps its digits. Measured against the host's library: within three or
four ulp. `sinh` and `tanh` of `-0.0` are `-0.0`.

`sinh` and `cosh` of more than 88 are worked out through half the
exponent, so that an answer below the largest float is found; one past
it is an overflow (`ERANGE`, an infinity of the right sign). An
infinite `x` answers an infinity, with no error. `tanh` is exactly 1,
with `x`'s sign, past 9.

### `asinh`, `acosh`, `atanh`, `asinhf`, `acoshf`, `atanhf`

    double asinh(double x);
    double acosh(double x);
    double atanh(double x);
    float  asinhf(float x);
    float  acoshf(float x);
    float  atanhf(float x);

The inverse hyperbolic functions, written as fdlibm writes them: each a
`log1p` of something that keeps its digits, and a plain `log` once `x`
is 4096 or more. No accuracy was measured for these beyond that of
`log` and `log1p` beneath them.

`acosh` below 1 is a domain error (`EDOM`, a NaN). `atanh` past 1
either way is a domain error, and of 1 or -1 is a pole (`ERANGE`, an
infinity of that sign). `asinh` has no error; of an infinity it is
that infinity.

### `erf`, `erfc`, `erff`, `erfcf`

    double erf(double x);
    double erfc(double x);
    float  erff(float x);
    float  erfcf(float x);

The error function, and its complement, `1 - erf(x)`, worked out on its
own so that it keeps its digits where `erf` is close to 1. Fitted and
measured against 40-digit values: `erf` within two ulp, and `erfc`
within four where its answer is a normal number, before the error of
the `exp` they call.

`erfc` is 0 from 10.1 up, with no error. `erf` of an infinity is 1
with its sign, and `erfc(-INFINITY)` is 2.

### `tgamma`, `lgamma`, `tgammaf`, `lgammaf`

    double tgamma(double x);
    double lgamma(double x);
    float  tgammaf(float x);
    float  lgammaf(float x);

The gamma function, and the natural logarithm of its magnitude. Fitted
and measured against 40-digit values: `tgamma` within seven ulp up to
where it overflows, and `lgamma` within three, before the error of the
`log` and `exp` beneath them. Close to the places below zero where
`lgamma` is zero, its relative error is not small.

`tgamma` of zero is a pole (`ERANGE`, an infinity with the zero's
sign); of a negative whole number or minus infinity, a domain error
(`EDOM`, a NaN); of more than 35.5, an overflow (`ERANGE`, `HUGE_VAL`).
`tgamma(INFINITY)` is an infinity.

`lgamma` of zero or of a negative whole number is a pole (`ERANGE`,
`HUGE_VAL`), and of either infinity is `HUGE_VAL` with no error.
`lgamma` does not say what the sign of the gamma was: there is no
`signgam`.

## `tgmath.h`

C99's type-generic mathematics: one name for the `float`, `double` and
`long double` versions of a `math.h` function, the macro choosing among
them by the types of its arguments. The header includes `math.h`, so
everything there is here too.

On acc the choice is always the same one. `float` and `double` are one
four-byte type, there is no `long double` and no `complex.h`, and an
integer argument would be taken as a `double` anyway. So each generic
macro is the `double` function itself, defined as its own name:

    #define sin(x)              sin(x)

A call converts each argument to `double` as any call to the function
does, and the answer is a `double`, which is a `float` too: a program
that writes `sqrt(f)` with a `float` `f` gets the answer `sqrtf(f)`
would. `#ifdef sin` is true after the header, as it is where the macro
is a real one.

Being function-like macros, they expand only where a `(` follows, so
`sin` on its own, as a function pointer, is the function, as it is
without the header.

### `acos`, `asin`, `atan`, `acosh`, `asinh`, `atanh`, `cos`, `sin`, `tan`, `cosh`, `sinh`, `tanh`

The trigonometric and hyperbolic functions and their inverses. Each is
the `math.h` function of the same name.

### `exp`, `log`, `pow`, `sqrt`, `fabs`

The five that C99 has for complex numbers as well, and so would also
choose `cexp`, `clog` and the rest. acc has no complex numbers, so
these are the `math.h` functions too.

### `ceil`, `floor`, `trunc`, `round`, `lround`, `llround`, `rint`, `lrint`, `llrint`, `nearbyint`

Rounding to a whole number, each the `math.h` function of the same
name. The `l` and `ll` ones answer a `long` or a `long long`, as there.

### `fmod`, `remainder`, `remquo`, `fdim`, `fmax`, `fmin`, `copysign`, `nextafter`

Remainders, differences, choices and steps, each the `math.h` function
of the same name. `remquo`'s third argument is its `int *`, as there.

### `frexp`, `ldexp`, `scalbn`, `scalbln`, `ilogb`, `logb`, `fma`

A number's exponent and scale, and the fused multiply and add, each the
`math.h` function of the same name. The `int *` of `frexp` and the
exponent of `ldexp`, `scalbn` and `scalbln` are taken as they are, not
chosen by.

### `cbrt`, `hypot`, `exp2`, `expm1`, `log2`, `log10`, `log1p`

Roots, powers and logarithms beyond the five above, each the `math.h`
function of the same name.

### `atan2`, `erf`, `erfc`, `tgamma`, `lgamma`

The angle of a point, the error function and the gamma function, each
the `math.h` function of the same name.

### `nexttoward`

Not here: its second argument is a `long double`, which acc refuses.
`nextafter` is the same step with a `double` direction.

## `fenv.h`

The floating-point environment: C99's exception flags and rounding
modes. On acc there is very little of one. The arithmetic is a library
of subroutines, not a unit with status flags, and it rounds to nearest,
ties to even, and in no other way. So the header says what C99 lets it
say: no exception is supported, and `FE_TONEAREST` is the one rounding
direction. Every function is there, and each does what can be done.

The header gives the types `fenv_t` (a structure of one unused `char`)
and `fexcept_t` (a `char`), and the macros `FE_ALL_EXCEPT`,
`FE_TONEAREST` and `FE_DFL_ENV`.

Errors in `math.h` are reported in `errno` alone (`math_errhandling` is
`MATH_ERRNO`), so a program that wants to know whether a function failed
sets `errno` to 0 and looks at it afterwards, rather than testing flags
here.

### `FE_ALL_EXCEPT`, `FE_DIVBYZERO`, `FE_INEXACT`, `FE_INVALID`, `FE_OVERFLOW`, `FE_UNDERFLOW`

    #define FE_ALL_EXCEPT   0

None of the five exception macros is defined, which is how C99 has an
implementation say it supports none of them, and `FE_ALL_EXCEPT`, the
set of all those supported, is 0. A program can ask:

    #ifdef FE_OVERFLOW
        feclearexcept(FE_OVERFLOW);
    #endif

A division by zero, an overflow or an invalid operation still has its
IEEE answer (an infinity or a NaN); nothing records that it happened.

### `FE_TONEAREST`, `FE_DOWNWARD`, `FE_UPWARD`, `FE_TOWARDZERO`

    #define FE_TONEAREST    0

To nearest, ties to even, is the one rounding direction, and its macro
the only one defined: `FE_DOWNWARD`, `FE_UPWARD` and `FE_TOWARDZERO` are
not. `FLT_ROUNDS`, in `float.h`, is 1, which says the same.

### `fegetround`, `fesetround`

    int fegetround(void);
    int fesetround(int round);

`fegetround` always answers `FE_TONEAREST`. `fesetround` answers 0 when
`round` is `FE_TONEAREST`, and nonzero, changing nothing, for any other
value, as C says a request for a direction that cannot be had must.

### `feclearexcept`, `feraiseexcept`, `fetestexcept`

    int feclearexcept(int excepts);
    int feraiseexcept(int excepts);
    int fetestexcept(int excepts);

There is never a flag set, so `fetestexcept` always answers 0.
`feclearexcept` and `feraiseexcept` do nothing, and answer 0 when
`excepts` is 0 (`FE_ALL_EXCEPT` among them) and nonzero, for failure,
otherwise.

### `fegetexceptflag`, `fesetexceptflag`, `fexcept_t`

    typedef char fexcept_t;

    int fegetexceptflag(fexcept_t *flagp, int excepts);
    int fesetexceptflag(const fexcept_t *flagp, int excepts);

`fegetexceptflag` stores 0 in `*flagp`; `fesetexceptflag` does nothing.
Both answer 0 when `excepts` is 0, and nonzero otherwise.

### `fegetenv`, `fesetenv`, `feholdexcept`, `feupdateenv`, `fenv_t`, `FE_DFL_ENV`

    typedef struct { char unused; } fenv_t;

    #define FE_DFL_ENV      (&__acc_fe_dfl_env)

    int fegetenv(fenv_t *envp);
    int fesetenv(const fenv_t *envp);
    int feholdexcept(fenv_t *envp);
    int feupdateenv(const fenv_t *envp);

The environment is only the rounding direction, which never changes, so
there is nothing to save or put back. `fegetenv` and `feholdexcept`
clear the one byte of `*envp`; `fesetenv` and `feupdateenv` read
nothing. All four answer 0, for success.

`FE_DFL_ENV` points to the default environment, a constant `fenv_t`,
which may be handed to `fesetenv` or `feupdateenv` like any other.

## `float.h`

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

### `FLT_RADIX`, `FLT_ROUNDS`, `FLT_EVAL_METHOD`, `DECIMAL_DIG`

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

### `FLT_MANT_DIG`, `FLT_DIG`, `FLT_MIN_EXP`, `FLT_MIN_10_EXP`, `FLT_MAX_EXP`, `FLT_MAX_10_EXP`, `FLT_MAX`, `FLT_EPSILON`, `FLT_MIN`

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

### `DBL_MANT_DIG`, `DBL_DIG`, `DBL_MIN_EXP`, `DBL_MIN_10_EXP`, `DBL_MAX_EXP`, `DBL_MAX_10_EXP`, `DBL_MAX`, `DBL_EPSILON`, `DBL_MIN`

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

### `LDBL_MANT_DIG`, `LDBL_DIG`, `LDBL_MIN_EXP`, `LDBL_MIN_10_EXP`, `LDBL_MAX_EXP`, `LDBL_MAX_10_EXP`, `LDBL_MAX`, `LDBL_EPSILON`, `LDBL_MIN`

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

## `limits.h`

The ranges of the integer types.

An `int` here is 24 bits, the width of a register and of an address, so
`INT_MAX` is 8388607 and not the 2147483647 a program written elsewhere
expects. A `long` is 32 bits and a `long long` 64, as agondev has them.
`char` is signed, as it is on this chip.

### `CHAR_BIT`

    #define CHAR_BIT 8

The bits in a byte, and so in a `char`.

### `CHAR_MIN`, `CHAR_MAX`, `SCHAR_MIN`, `SCHAR_MAX`, `UCHAR_MAX`

| Macro       | Value | Type            |
|-------------|-------|-----------------|
| `SCHAR_MIN` | -128  | `signed char`   |
| `SCHAR_MAX` | 127   | `signed char`   |
| `UCHAR_MAX` | 255   | `unsigned char` |
| `CHAR_MIN`  | -128  | `char`          |
| `CHAR_MAX`  | 127   | `char`          |

`char` is signed, so `CHAR_MIN` and `CHAR_MAX` are defined as
`SCHAR_MIN` and `SCHAR_MAX`.

### `SHRT_MIN`, `SHRT_MAX`, `USHRT_MAX`

| Macro       | Value  | Type             |
|-------------|--------|------------------|
| `SHRT_MIN`  | -32768 | `short`          |
| `SHRT_MAX`  | 32767  | `short`          |
| `USHRT_MAX` | 65535  | `unsigned short` |

### `INT_MIN`, `INT_MAX`, `UINT_MAX`

| Macro      | Value     | Written as   |
|------------|-----------|--------------|
| `INT_MIN`  | -8388608  | `(-8388608)` |
| `INT_MAX`  | 8388607   | `8388607`    |
| `UINT_MAX` | 16777215  | `16777215u`  |

An `int` is 24 bits. A program that assumes 32 (that `4096 * 4096` is
16777216 and not 0, or that an `int` holds the milliseconds in a day)
needs a `long`.

### `LONG_MIN`, `LONG_MAX`, `ULONG_MAX`

| Macro       | Value       | Written as             |
|-------------|-------------|------------------------|
| `LONG_MIN`  | -2147483648 | `(-2147483647L - 1L)`  |
| `LONG_MAX`  | 2147483647  | `2147483647L`          |
| `ULONG_MAX` | 4294967295  | `4294967295UL`         |

### `LLONG_MIN`, `LLONG_MAX`, `ULLONG_MAX`

| Macro        | Value                | Written as                       |
|--------------|----------------------|----------------------------------|
| `LLONG_MIN`  | -9223372036854775808 | `(-9223372036854775807LL - 1LL)` |
| `LLONG_MAX`  | 9223372036854775807  | `9223372036854775807LL`          |
| `ULLONG_MAX` | 18446744073709551615 | `18446744073709551615ULL`        |

### `MB_LEN_MAX`

    #define MB_LEN_MAX 3

The most bytes a multibyte character takes. Multibyte characters are
UTF-8, as far as a 16-bit `wchar_t` goes: one to three bytes, up to
U+FFFF, and a four-byte sequence is not one. See
[`mblen`](5-program.md#mblen-mbtowc-wctomb-mb_cur_max).

## `stdint.h`

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

### `int8_t`, `int16_t`, `int24_t`, `int32_t`, `int64_t`

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

### `uint8_t`, `uint16_t`, `uint24_t`, `uint32_t`, `uint64_t`

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

### `INT8_MAX`, `INT16_MAX`, `INT24_MAX`, `INT32_MAX`, `INT64_MAX`

    #define INT8_MAX     127
    #define INT16_MAX    32767
    #define INT24_MAX    8388607
    #define INT32_MAX    2147483647L
    #define INT64_MAX    9223372036854775807LL

The most each signed type of an exact width holds. Each is written with
the type a value of that width promotes to: an `int` up to 24 bits, then
a `long` and a `long long`.

### `INT8_MIN`, `INT16_MIN`, `INT24_MIN`, `INT32_MIN`, `INT64_MIN`

    #define INT8_MIN     (-128)
    #define INT16_MIN    (-32768)
    #define INT24_MIN    (-8388608)
    #define INT32_MIN    (-2147483647L - 1L)
    #define INT64_MIN    (-9223372036854775807LL - 1LL)

The least each signed type of an exact width holds. The 32- and 64-bit
ones are written as a subtraction, since the positive half of the
number is past the type's range.

### `UINT8_MAX`, `UINT16_MAX`, `UINT24_MAX`, `UINT32_MAX`, `UINT64_MAX`

    #define UINT8_MAX    255
    #define UINT16_MAX   65535
    #define UINT24_MAX   16777215u
    #define UINT32_MAX   4294967295UL
    #define UINT64_MAX   18446744073709551615ULL

The most each unsigned type of an exact width holds. `UINT8_MAX` and
`UINT16_MAX` are plain `int` constants, since a `uint8_t` or `uint16_t`
promotes to an `int`.

### `int_least8_t`, `int_fast8_t`, `uint_least8_t`, `uint_fast8_t`

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

### `intptr_t`, `uintptr_t`, `INTPTR_MAX`, `UINTPTR_MAX`

    typedef int                intptr_t;
    typedef unsigned int       uintptr_t;

Integers that hold a pointer: an address is three bytes, and so is an
`int`. `INTPTR_MIN` and `INTPTR_MAX` are -8388608 and 8388607, and
`UINTPTR_MAX` is 16777215.

### `intmax_t`, `uintmax_t`, `INTMAX_MAX`, `UINTMAX_MAX`

    typedef long long          intmax_t;
    typedef unsigned long long uintmax_t;

The widest integers: 64 bits. `INTMAX_MIN`, `INTMAX_MAX` and
`UINTMAX_MAX` are `INT64_MIN`, `INT64_MAX` and `UINT64_MAX`.
[`inttypes.h`](#inttypesh) has the functions over them.

### `SIZE_MAX`, `PTRDIFF_MIN`, `PTRDIFF_MAX`, `WCHAR_MIN`, `WCHAR_MAX`

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

### `INT32_C`, `UINT32_C`, `INT64_C`, `UINT64_C`, `INTMAX_C`, `UINTMAX_C`

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

## `inttypes.h`

The `printf` and `scanf` conversions for the types of
[`stdint.h`](#stdinth), and functions over `intmax_t`, which is a
`long long` here.

The header includes `stdint.h`, so it gives all of that header's types
and macros too.

### `PRId32`, `PRIu32`, `PRIx32`, `PRId64`, `PRIu64`, `PRIx64`, `PRIu8`

The `printf` conversions, as strings to put after a `%`:

    uint32_t size = 100000;
    int16_t delta = -5;

    printf("%" PRIu32 " bytes, %" PRId16 "\n", size, delta);

Each name is `PRI`, a conversion letter (`d`, `i`, `o`, `u`, `x` or
`X`), and a type: a width for the exact type (`PRIu32`), `LEAST` or
`FAST` and the width (`PRIuLEAST32`, `PRIuFAST32`), `MAX` for `intmax_t`
(`PRIuMAX`), or `PTR` for `intptr_t` (`PRIuPTR`). The widths are 8, 16,
24, 32 and 64, the 24 only for the exact type; `PRId24` and its family
are not C99's.

The letter is the conversion; the type sets the length in front of it:

| Type of the macro                       | Length | `PRId...` is |
|-----------------------------------------|--------|--------------|
| 8 and 16 bits, exact, least and fast    | none   | `"d"`        |
| 24 bits, `PTR`                          | none   | `"d"`        |
| 32 bits, exact, least and fast          | `l`    | `"ld"`       |
| 64 bits, exact, least and fast, `MAX`   | `ll`   | `"lld"`      |

A value narrower than an `int` arrives at `printf` as an `int`, so the
8- and 16-bit ones have no length. That makes `PRIx8` and `PRIx16` write
a negative `int8_t` or `int16_t` as a 24-bit `unsigned int`: `-1` is
`ffffff`. Converting it to `uint8_t` or `uint16_t` first writes `ff` or
`ffff`.

See [`printf`](2-io.md#printf-fprintf-sprintf-snprintf) for the
conversions and lengths themselves.

### `SCNd32`, `SCNu32`, `SCNx32`, `SCNd64`, `SCNu64`, `SCNx64`, `SCNx8`

The `scanf` conversions, named the same way as the `PRI` ones with `SCN`
in front, for the letters `d`, `i`, `o`, `u` and `x` (C99 has no
`SCNX`):

    uint8_t red, green, blue;           /* from "ff8000" */

    if (sscanf(text, "%2" SCNx8 "%2" SCNx8 "%2" SCNx8,
               &red, &green, &blue) == 3)
        set_colour(red, green, blue);

`scanf` stores through a pointer, so it is told every width:

| Type of the macro                       | Length | `SCNd...` is |
|-----------------------------------------|--------|--------------|
| 8 bits, exact, least and fast           | `hh`   | `"hhd"`      |
| 16 bits, exact, least and fast          | `h`    | `"hd"`       |
| 24 bits, `PTR`                          | none   | `"d"`        |
| 32 bits, exact, least and fast          | `l`    | `"ld"`       |
| 64 bits, exact, least and fast, `MAX`   | `ll`   | `"lld"`      |

See [`scanf`](2-io.md#scanf-fscanf-sscanf) for how each reads.

### `PRIdMAX`, `PRIuMAX`, `PRIxMAX`, `PRIdPTR`, `PRIuPTR`, `PRIxPTR`

The conversions for `intmax_t` and `uintmax_t` (`"lld"`, `"llu"`,
`"llx"` and the others with `ll`), and for `intptr_t` and `uintptr_t`
(`"d"`, `"u"`, `"x"` and the others with no length, since those are
`int`s). `SCNdMAX`, `SCNuMAX`, `SCNdPTR` and the rest are the same
strings.

### `imaxabs`

    intmax_t imaxabs(intmax_t j);

The magnitude of `j`. `INTMAX_MIN` has no magnitude an `intmax_t` holds.

### `imaxdiv`, `imaxdiv_t`

    typedef struct {
        intmax_t quot;
        intmax_t rem;
    } imaxdiv_t;

    imaxdiv_t imaxdiv(intmax_t numer, intmax_t denom);

`numer / denom` in `quot` and `numer % denom` in `rem`, both at once,
as [`lldiv`](5-program.md#div-ldiv-lldiv-div_t-ldiv_t-lldiv_t) has them:
the quotient is truncated toward zero, so `rem` has `numer`'s sign. It
is the two operators and nothing more, so `denom` must not be 0.

### `strtoimax`, `strtoumax`

    intmax_t  strtoimax(const char *s, char **end, int base);
    uintmax_t strtoumax(const char *s, char **end, int base);

`strtoll` and `strtoull` under the names of `intmax_t`: they call those
and answer what they answer. See
[`strtol`](5-program.md#strtol-strtoul-strtoll-strtoull) for the bases,
`*end`, and `ERANGE` with `INTMAX_MIN`, `INTMAX_MAX` or `UINTMAX_MAX`
for a number past the range.

### `wcstoimax`, `wcstoumax`

    intmax_t  wcstoimax(const wchar_t *s, wchar_t **end, int base);
    uintmax_t wcstoumax(const wchar_t *s, wchar_t **end, int base);

`strtoimax` and `strtoumax` over a wide string: the same reader, a
`wchar_t` (16 bits) at a time. The spaces, sign, `0x` and digits are
the same characters as `strtol` takes; any other wide character ends the
number. `*end` is left on a `wchar_t` of `s`.

The header declares them without the name `wchar_t`, over a `short`
that is the same type, since C99 does not give `inttypes.h` that name.
A program that writes `wchar_t` itself includes `stddef.h` or `wchar.h`
for it.
