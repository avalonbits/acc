/* <fenv.h>: what a program can ask of it portably -- the rounding mode, and
 * the functions over the set of every exception, which here is empty. */
#include <fenv.h>
#include <stdio.h>

#pragma STDC FENV_ACCESS ON

int main(void)
{
    fenv_t env;
    fexcept_t flags;
    volatile double x = 1.0;

    printf("fegetround %d\n", fegetround() == FE_TONEAREST);
    printf("fesetround nearest %d\n", fesetround(FE_TONEAREST));
    printf("fesetround -1 fails %d\n", fesetround(-1) != 0);
    printf("still nearest %d\n", fegetround() == FE_TONEAREST);

    printf("feclearexcept %d\n", feclearexcept(FE_ALL_EXCEPT));
    printf("fetestexcept %d\n", fetestexcept(FE_ALL_EXCEPT));
    printf("feraiseexcept 0 %d\n", feraiseexcept(0));
    printf("fegetexceptflag %d\n", fegetexceptflag(&flags, FE_ALL_EXCEPT));
    printf("fesetexceptflag %d\n", fesetexceptflag(&flags, FE_ALL_EXCEPT));

    printf("fegetenv %d\n", fegetenv(&env));
    printf("feholdexcept %d\n", feholdexcept(&env));
    x = x / 3.0;
    printf("fesetenv %d\n", fesetenv(&env));
    printf("feupdateenv %d\n", feupdateenv(&env));
    printf("fesetenv dfl %d\n", fesetenv(FE_DFL_ENV));
    printf("x %d\n", x > 0.33 && x < 0.34);

    return 0;
}
