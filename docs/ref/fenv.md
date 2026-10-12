# `fenv.h`

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

## `FE_ALL_EXCEPT`, `FE_DIVBYZERO`, `FE_INEXACT`, `FE_INVALID`, `FE_OVERFLOW`, `FE_UNDERFLOW`

    #define FE_ALL_EXCEPT   0

None of the five exception macros is defined, which is how C99 has an
implementation say it supports none of them, and `FE_ALL_EXCEPT`, the
set of all those supported, is 0. A program can ask:

    #ifdef FE_OVERFLOW
        feclearexcept(FE_OVERFLOW);
    #endif

A division by zero, an overflow or an invalid operation still has its
IEEE answer (an infinity or a NaN); nothing records that it happened.

## `FE_TONEAREST`, `FE_DOWNWARD`, `FE_UPWARD`, `FE_TOWARDZERO`

    #define FE_TONEAREST    0

To nearest, ties to even, is the one rounding direction, and its macro
the only one defined: `FE_DOWNWARD`, `FE_UPWARD` and `FE_TOWARDZERO` are
not. `FLT_ROUNDS`, in `float.h`, is 1, which says the same.

## `fegetround`, `fesetround`

    int fegetround(void);
    int fesetround(int round);

`fegetround` always answers `FE_TONEAREST`. `fesetround` answers 0 when
`round` is `FE_TONEAREST`, and nonzero, changing nothing, for any other
value, as C says a request for a direction that cannot be had must.

## `feclearexcept`, `feraiseexcept`, `fetestexcept`

    int feclearexcept(int excepts);
    int feraiseexcept(int excepts);
    int fetestexcept(int excepts);

There is never a flag set, so `fetestexcept` always answers 0.
`feclearexcept` and `feraiseexcept` do nothing, and answer 0 when
`excepts` is 0 (`FE_ALL_EXCEPT` among them) and nonzero, for failure,
otherwise.

## `fegetexceptflag`, `fesetexceptflag`, `fexcept_t`

    typedef char fexcept_t;

    int fegetexceptflag(fexcept_t *flagp, int excepts);
    int fesetexceptflag(const fexcept_t *flagp, int excepts);

`fegetexceptflag` stores 0 in `*flagp`; `fesetexceptflag` does nothing.
Both answer 0 when `excepts` is 0, and nonzero otherwise.

## `fegetenv`, `fesetenv`, `feholdexcept`, `feupdateenv`, `fenv_t`, `FE_DFL_ENV`

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
