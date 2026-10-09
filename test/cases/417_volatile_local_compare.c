/* gcc.c-torture/execute/20170419-1: two volatile locals compared in a loop,
 * left in their slots by the SSA form's backends. */
#define INT_MAX 0x7fffffff
#define INT_MIN (-INT_MAX-1)

int x;

int main(void)
{
    volatile int a = 0;
    volatile int b = -INT_MAX;
    int j;

    for (j = 0; j < 18; j += 1)
        x = ((a == 0) != (b - (int) (INT_MIN)));

    return x == 0 ? 42 : 1;
}
