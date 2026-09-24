#include <stdio.h>
#include <agon/mos.h>
int main(void)
{
    printf("hello %d %d\n", getsysvar_scrCols() > 0, sys_vars->scrCols == getsysvar_scrCols());
    return 0;
}
