# `fenv.h`

The floating-point environment: C99's exception flags and rounding
modes. On acc there is very little of one. The arithmetic is a library
of subroutines, not a unit with status flags, and it rounds to nearest,
ties to even, and in no other way. The header names the five exceptions
and four rounding directions agondev's does, at its values, so that a
program written for agondev builds. The exception flags are kept by the
functions here, and no arithmetic raises one, as none of agondev's does;
of the directions, only `FE_TONEAREST` can be set.

The header gives the types `fenv_t` (a structure holding the flags) and
`fexcept_t` (an `unsigned char` of flags), and the macros below and
`FE_DFL_ENV`.

Errors in `math.h` are reported in `errno` alone (`math_errhandling` is
`MATH_ERRNO`), so a program that wants to know whether a function failed
sets `errno` to 0 and looks at it afterwards, rather than testing flags
here.

## `FE_ALL_EXCEPT`, `FE_DIVBYZERO`, `FE_INEXACT`, `FE_INVALID`, `FE_OVERFLOW`, `FE_UNDERFLOW`

    #define FE_DIVBYZERO    (1 << 6)
    #define FE_INEXACT      (1 << 5)
    #define FE_INVALID      (1 << 4)
    #define FE_OVERFLOW     (1 << 3)
    #define FE_UNDERFLOW    (1 << 2)
    #define FE_ALL_EXCEPT   (FE_DIVBYZERO | FE_INEXACT | FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW)

agondev's values. A flag is set only by `feraiseexcept` or
`fesetexceptflag`: a division by zero, an overflow or an invalid
operation has its IEEE answer (an infinity or a NaN), and nothing records
that it happened.

## `FE_TONEAREST`, `FE_DOWNWARD`, `FE_UPWARD`, `FE_TOWARDZERO`

    #define FE_TONEAREST    0
    #define FE_TOWARDZERO   1
    #define FE_DOWNWARD     2
    #define FE_UPWARD       3

To nearest, ties to even, is the one rounding direction there is. The
other three are named so that a program written for agondev builds, and
asking for one fails. `FLT_ROUNDS`, in `float.h`, is 1.

## `fegetround`, `fesetround`

    int fegetround(void);
    int fesetround(int round);

`fegetround` always answers `FE_TONEAREST`. `fesetround` answers 0 when
`round` is `FE_TONEAREST`, and nonzero, changing nothing, for any other
value, as C says a request for a direction that cannot be had must.
agondev's takes any value and answers 0, though its arithmetic rounds to
nearest whatever it was told.

## `feclearexcept`, `feraiseexcept`, `fetestexcept`

    int feclearexcept(int excepts);
    int feraiseexcept(int excepts);
    int fetestexcept(int excepts);

`feraiseexcept` sets the flags in `excepts`, `feclearexcept` clears them,
and `fetestexcept` answers which of them are set. A bit of `excepts`
outside `FE_ALL_EXCEPT` is no exception: the first two do what they can
with the rest and answer nonzero, and answer 0 otherwise.

    feraiseexcept(FE_OVERFLOW);
    if (fetestexcept(FE_OVERFLOW))      /* true */
        feclearexcept(FE_OVERFLOW);

## `fegetexceptflag`, `fesetexceptflag`, `fexcept_t`

    typedef unsigned char fexcept_t;

    int fegetexceptflag(fexcept_t *flagp, int excepts);
    int fesetexceptflag(const fexcept_t *flagp, int excepts);

`fegetexceptflag` stores the flags of `excepts` that are set in `*flagp`;
`fesetexceptflag` sets the flags of `excepts` to what `*flagp` holds of
them, raising and clearing. `fegetexceptflag` answers nonzero for a bit
outside `FE_ALL_EXCEPT`, and both answer 0 otherwise.

## `fegetenv`, `fesetenv`, `feholdexcept`, `feupdateenv`, `fenv_t`, `FE_DFL_ENV`

    typedef struct { unsigned char flags; } fenv_t;

    #define FE_DFL_ENV      (&__acc_fe_dfl_env)

    int fegetenv(fenv_t *envp);
    int fesetenv(const fenv_t *envp);
    int feholdexcept(fenv_t *envp);
    int feupdateenv(const fenv_t *envp);

The environment is the flags, the rounding direction never changing.
`fegetenv` saves them in `*envp`; `feholdexcept` saves them and clears
them all; `fesetenv` puts back what `*envp` holds; `feupdateenv` puts it
back and keeps raised what was raised since. All four answer 0.

`FE_DFL_ENV` points to the default environment, a constant `fenv_t` with
no flag set, which may be handed to `fesetenv` or `feupdateenv` like any
other.
