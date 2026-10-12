# `math.h`

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

## `float_t`, `double_t`

    typedef float  float_t;
    typedef double double_t;

The types an expression of `float` and of `double` is worked out in.
Each is the type itself, since `FLT_EVAL_METHOD` is 0: every operation
is carried out in the type it is given. Both are four bytes.

## `math_errhandling`, `MATH_ERRNO`, `MATH_ERREXCEPT`

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

## `HUGE_VAL`, `HUGE_VALF`, `INFINITY`, `NAN`

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

## `fpclassify`, `isnan`, `isinf`, `isfinite`, `isnormal`, `signbit`, `FP_NAN`, `FP_INFINITE`, `FP_ZERO`, `FP_SUBNORMAL`, `FP_NORMAL`

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

## `isgreater`, `isgreaterequal`, `isless`, `islessequal`, `islessgreater`, `isunordered`

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

## `fabs`, `fabsf`, `copysign`, `copysignf`

    double fabs(double x);
    double copysign(double x, double y);
    float  fabsf(float x);
    float  copysignf(float x, float y);

`fabs` is `x` with its sign bit cleared, and `copysign` is `x`'s
magnitude with `y`'s sign bit. Both work on the bits, so they are exact
for every value, and keep a NaN a NaN: `fabs(-0.0)` is `+0.0`, and
`copysign(1, -0.0)` is -1.

## `floor`, `ceil`, `trunc`, `floorf`, `ceilf`, `truncf`

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

## `round`, `roundf`, `lround`, `lroundf`, `llround`, `llroundf`

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

## `rint`, `nearbyint`, `rintf`, `nearbyintf`, `lrint`, `lrintf`, `llrint`, `llrintf`

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

## `fmod`, `fmodf`

    double fmod(double x, double y);
    float  fmodf(float x, float y);

What is left of `x` when whole multiples of `y` are taken from it, with
`x`'s sign: `fmod(7, 3)` is 1 and `fmod(-7, 3)` is -1. Exact for every
value; each subtraction it makes loses nothing.

A `y` of zero, or an infinite `x`, is a domain error (`EDOM`, a NaN). An
infinite `y`, or an `x` smaller than `y`, answers `x`.

## `remainder`, `remquo`, `remainderf`, `remquof`

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

## `modf`, `modff`

    double modf(double x, double *ip);
    float  modff(float x, float *ip);

Split `x` into its whole part, stored in `*ip`, and its fraction, which
is the answer. Both have `x`'s sign: `modf(-3.5, &w)` answers -0.5 and
stores -3, and `modf(-3.0, &w)` answers `-0.0`. An infinity stores
itself and answers a zero. Exact.

## `frexp`, `ldexp`, `frexpf`, `ldexpf`

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

## `scalbn`, `scalbln`, `scalbnf`, `scalblnf`

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

## `ilogb`, `ilogbf`, `logb`, `logbf`, `FP_ILOGB0`, `FP_ILOGBNAN`

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

## `fdim`, `fdimf`

    double fdim(double x, double y);
    float  fdimf(float x, float y);

`x - y` when `x` is the larger, and 0 otherwise. A NaN on either side
answers 0.

## `fmax`, `fmin`, `fmaxf`, `fminf`

    double fmax(double x, double y);
    double fmin(double x, double y);
    float  fmaxf(float x, float y);
    float  fminf(float x, float y);

The larger, or the smaller, of the two. A NaN on one side is ignored and
the other is the answer; with a NaN on both, the answer is a NaN.

## `nextafter`, `nextafterf`

    double nextafter(double x, double y);
    float  nextafterf(float x, float y);

The float next to `x` in the direction of `y`, one step of the bit
pattern: `nextafter(1, 2)` is `1 + FLT_EPSILON`. When `x` equals `y` the
answer is `y`; from a zero it is the smallest subnormal, with `y`'s
sign. A NaN on either side answers a NaN. A step past the largest float
is an overflow (`ERANGE`, an infinity).

There is no `nexttoward`, whose second argument is a `long double`.

## `nan`, `nanf`

    double nan(const char *tag);
    float  nanf(const char *tag);

The quiet NaN, bits `0x7fc00000`. `tag` is not read: every NaN these
make is the same one.

## `fma`, `fmaf`

    double fma(double x, double y, double z);
    float  fmaf(float x, float y, float z);

`x * y + z`, rounded once: the product is kept whole, in integers, and
only the sum is rounded, to nearest with ties to even. An answer that
overflows is an `ERANGE` infinity. When the product and `z` cancel
exactly the answer is `+0.0`.

A zero, an infinity or a NaN among the three is left to the ordinary
arithmetic, where `x * y` is then exact or a NaN.

It is worked out in 64-bit integers, so it is slower than `x * y + z`.

## `sqrt`, `sqrtf`

    double sqrt(double x);
    float  sqrtf(float x);

The square root, exact: the answer is the true root rounded to nearest,
ties to even. It was checked against the host's own square root for
every positive float, normal and subnormal.

It is worked out a bit at a time, a compare, a subtract and a shift per
bit, with no division. `sqrt(-0.0)` is `-0.0` and `sqrt(INFINITY)` is
an infinity. Any number below zero, minus infinity among them, is a
domain error (`EDOM`, a NaN).

## `cbrt`, `cbrtf`

    double cbrt(double x);
    float  cbrtf(float x);

The cube root, within one ulp over the whole range. A negative `x` has
a negative root, so it is never an error. A zero, an infinity or a NaN
is answered as it is.

## `hypot`, `hypotf`

    double hypot(double x, double y);
    float  hypotf(float x, float y);

The square root of `x * x + y * y`, within one ulp. Both are scaled
first, so the squares cannot overflow on the way to an answer that
does not: `hypot(3e30, 4e30)` is about `5e30`, though `3e30 * 3e30`
alone is an infinity. An answer too large is an overflow (`ERANGE`, an
infinity).

When the larger of the two is an infinity or a NaN, it is the answer.

## `exp`, `exp2`, `expm1`, `expf`, `exp2f`, `expm1f`

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

## `log`, `log2`, `log10`, `log1p`, `logf`, `log2f`, `log10f`, `log1pf`

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

## `pow`, `powf`

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
  infinity and an odd `y`), and below zero answers 0.
- An answer too large is an overflow (`ERANGE`, an infinity); one too
  small is zero, with no error.

An infinite `y` with a finite `x` other than 1 answers a NaN, where C99
asks for 0 or an infinity.

## `sin`, `cos`, `tan`, `sinf`, `cosf`, `tanf`

    double sin(double x);
    double cos(double x);
    double tan(double x);
    float  sinf(float x);
    float  cosf(float x);
    float  tanf(float x);

The sine, cosine and tangent of `x` radians. `x` is brought down to
within pi/4 of a multiple of pi/2, with pi/2 taken away in five pieces
so that the remainder keeps its digits.

Measured against the host's library: `sin` within two ulp over -100 to
100, and `cos` within eight, the eight being where the cosine passes
through zero. Past about 100000 the reduction runs out of digits, and
the answer degrades.

An infinity is a domain error (`EDOM`, a NaN).

## `asin`, `acos`, `atan`, `asinf`, `acosf`, `atanf`

    double asin(double x);
    double acos(double x);
    double atan(double x);
    float  asinf(float x);
    float  acosf(float x);
    float  atanf(float x);

The arc sine (from -pi/2 to pi/2), arc cosine (from 0 to pi) and arc
tangent (from -pi/2 to pi/2), in radians. Measured against the host's
library: `atan` and `asin` within two ulp, `acos` within one.

`asin` and `acos` of a number past 1 either way are a domain error
(`EDOM`, a NaN). `atan` of an infinity is pi/2 with its sign.

## `atan2`, `atan2f`

    double atan2(double y, double x);
    float  atan2f(float y, float x);

The angle of the point (`x`, `y`) from the positive `x` axis, from -pi
to pi: `atan2(1, -1)` is 3pi/4. It is `atan(y / x)`, moved into the
right half of the circle by the signs.

When `x` is zero, the answer is pi/2 when `y` is above zero and -pi/2
otherwise. When both are zero, the signs of the zeros decide, as C99
has it: `atan2(0.0, -0.0)` is pi, `atan2(-0.0, -0.0)` is -pi, and with
`x` `+0.0` the answer is `y`'s zero.

Two infinities answer a NaN, where C99 asks for an odd multiple of
pi/4.

## `sinh`, `cosh`, `tanh`, `sinhf`, `coshf`, `tanhf`

    double sinh(double x);
    double cosh(double x);
    double tanh(double x);
    float  sinhf(float x);
    float  coshf(float x);
    float  tanhf(float x);

The hyperbolic sine, cosine and tangent, written so that a small `x`
keeps its digits. Measured against the host's library: within three or
four ulp.

`sinh` and `cosh` of more than 88 are worked out through half the
exponent, so that an answer below the largest float is found; one past
it is an overflow (`ERANGE`, an infinity of the right sign). An
infinite `x` answers an infinity, with no error. `tanh` is exactly 1,
with `x`'s sign, past 9.

## `asinh`, `acosh`, `atanh`, `asinhf`, `acoshf`, `atanhf`

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

## `erf`, `erfc`, `erff`, `erfcf`

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

## `tgamma`, `lgamma`, `tgammaf`, `lgammaf`

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
