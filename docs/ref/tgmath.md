# `tgmath.h`

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

## `acos`, `asin`, `atan`, `acosh`, `asinh`, `atanh`, `cos`, `sin`, `tan`, `cosh`, `sinh`, `tanh`

The trigonometric and hyperbolic functions and their inverses. Each is
the `math.h` function of the same name.

## `exp`, `log`, `pow`, `sqrt`, `fabs`

The five that C99 has for complex numbers as well, and so would also
choose `cexp`, `clog` and the rest. acc has no complex numbers, so
these are the `math.h` functions too.

## `ceil`, `floor`, `trunc`, `round`, `lround`, `llround`, `rint`, `lrint`, `llrint`, `nearbyint`

Rounding to a whole number, each the `math.h` function of the same
name. The `l` and `ll` ones answer a `long` or a `long long`, as there.

## `fmod`, `remainder`, `remquo`, `fdim`, `fmax`, `fmin`, `copysign`, `nextafter`

Remainders, differences, choices and steps, each the `math.h` function
of the same name. `remquo`'s third argument is its `int *`, as there.

## `frexp`, `ldexp`, `scalbn`, `scalbln`, `ilogb`, `logb`, `fma`

A number's exponent and scale, and the fused multiply and add, each the
`math.h` function of the same name. The `int *` of `frexp` and the
exponent of `ldexp`, `scalbn` and `scalbln` are taken as they are, not
chosen by.

## `cbrt`, `hypot`, `exp2`, `expm1`, `log2`, `log10`, `log1p`

Roots, powers and logarithms beyond the five above, each the `math.h`
function of the same name.

## `atan2`, `erf`, `erfc`, `tgamma`, `lgamma`

The angle of a point, the error function and the gamma function, each
the `math.h` function of the same name.

## `nexttoward`

Not here: its second argument is a `long double`, which acc refuses.
`nextafter` is the same step with a `double` direction.
