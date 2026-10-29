/* <fenv.h> against libagon's: the exception flags, set, tested and
 * cleared, and the environment saved and put back. Neither library's
 * arithmetic raises one; the flags are what the program sets.
 *
 * The rounding mode is acc's own: libagon's fesetround takes any of the
 * four, and any number at all, and its arithmetic rounds to nearest
 * whatever it was told. acc's takes FE_TONEAREST alone and fails for the
 * rest, as C says a request for a direction it cannot give must; what it
 * answers is in the table, under AGONDEV. */
#include <fenv.h>
#include <stdio.h>

int main(void)
{
    fenv_t env;
    fexcept_t flags;

    printf("modes %d %d %d %d all %d\n", FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO, FE_ALL_EXCEPT);
    printf("round %d\n", fegetround());
#ifndef AGONDEV
    printf("set up %d now %d\n", fesetround(FE_UPWARD) != 0, fegetround());
    printf("set bad %d now %d\n", fesetround(12345) != 0, fegetround());
#else
    printf("set up 1 now 0\n");
    printf("set bad 1 now 0\n");
#endif
    fesetround(FE_TONEAREST);
    printf("clear %d test %d\n", feclearexcept(FE_ALL_EXCEPT), fetestexcept(FE_ALL_EXCEPT));
    printf("raise %d test %d\n", feraiseexcept(FE_DIVBYZERO | FE_INEXACT), fetestexcept(FE_ALL_EXCEPT));
    printf("getflag %d", fegetexceptflag(&flags, FE_ALL_EXCEPT));
    feclearexcept(FE_ALL_EXCEPT);
    printf(" setflag %d test %d\n", fesetexceptflag(&flags, FE_DIVBYZERO), fetestexcept(FE_ALL_EXCEPT));
    printf("getenv %d", fegetenv(&env));
    fesetround(FE_DOWNWARD);
    printf(" setenv %d round %d\n", fesetenv(&env), fegetround());
    printf("hold %d test %d", feholdexcept(&env), fetestexcept(FE_ALL_EXCEPT));
    feraiseexcept(FE_OVERFLOW);
    printf(" update %d test %d\n", feupdateenv(&env), fetestexcept(FE_ALL_EXCEPT));
    return 0;
}
