/* <setjmp.h>: back to setjmp from as deep as a program likes, with the
 * frame the caller had and the value longjmp was given. */
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static jmp_buf top, inner, bufs[3];
static int depth;

static void dive(int n, int val)
{
    char pad[20];

    memset(pad, n, sizeof pad);
    depth = n;
    if (n == 0)
        longjmp(top, val);
    dive(n - 1, val);
    printf("never %d\n", pad[0]);
}

static int vla(int n)
{
    int a[n];
    int i;

    for (i = 0; i < n; i++)
        a[i] = i * i;
    if (n > 4)
        longjmp(inner, a[n - 1]);

    return a[n - 1];
}

static int through(void)
{
    volatile int kept = 5;
    int r;

    if ((r = setjmp(inner)) != 0)
        return r + kept;
    kept = 7;
    vla(3);
    vla(6);

    return -1;
}

static void (*jump)(jmp_buf, int) = longjmp;

int main(void)
{
    volatile int count = 0;
    int r, i;

    r = setjmp(top);
    printf("setjmp %d, count %d, depth %d\n", r, count, depth);
    if (count++ < 4)
        dive(count * 3, count == 2 ? 0 : count * 100);

    printf("through %d\n", through());

    switch (setjmp(top)) {
    case 0:
        printf("switch first\n");
        longjmp(top, 2);
    case 2:
        printf("switch second\n");
        break;
    default:
        printf("switch wrong\n");
    }

    if (!setjmp(top)) {
        printf("not first\n");
        jump(top, -1);
    } else {
        printf("not second\n");
    }

    for (i = 0; i < 3; i++)
        if (setjmp(bufs[i]) == 0)
            printf("buf %d set\n", i);
        else
            printf("buf %d back\n", i);
    count = 0;
    if (setjmp(top) < 3) {
        printf("comparison %d\n", count);
        longjmp(top, ++count);
    }
    printf("done %d\n", count);

    return 0;
}
