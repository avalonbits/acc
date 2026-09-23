/* <signal.h>: signal and raise, and abort's SIGABRT. Every handler here
 * sets itself again before it returns, which reads the same whether the
 * library puts a signal back to SIG_DFL before calling its handler, as
 * acc's does, or leaves it set, as glibc's does. */
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

static volatile sig_atomic_t seen[16];
static jmp_buf back;

static void count(int sig)
{
    seen[sig]++;
    signal(sig, count);
}

static void escape(int sig)
{
    printf("escape %d\n", sig == SIGABRT);
    longjmp(back, sig);
}

int main(void)
{
    void (*old)(int);
    int r;

    printf("distinct %d\n", SIGINT != SIGTERM && SIGABRT != SIGFPE
                            && SIGILL != SIGSEGV && SIG_DFL != SIG_IGN
                            && SIG_ERR != SIG_DFL && SIG_ERR != SIG_IGN);

    old = signal(SIGINT, count);
    printf("first old is default %d\n", old == SIG_DFL);
    printf("raise %d\n", raise(SIGINT));
    printf("raise %d\n", raise(SIGINT));
    printf("seen %d\n", (int) seen[SIGINT]);
    old = signal(SIGINT, SIG_IGN);
    printf("old is count %d\n", old == count);
    printf("ignored raise %d\n", raise(SIGINT));
    printf("seen %d\n", (int) seen[SIGINT]);
    printf("old is ignore %d\n", signal(SIGINT, SIG_DFL) == SIG_IGN);

    signal(SIGTERM, count);
    signal(SIGFPE, count);
    raise(SIGTERM);
    raise(SIGFPE);
    raise(SIGFPE);
    printf("term %d fpe %d int %d\n", (int) seen[SIGTERM], (int) seen[SIGFPE],
           (int) seen[SIGINT]);

    /* abort, with a handler that does not return. */
    signal(SIGABRT, escape);
    if ((r = setjmp(back)) == 0)
        abort();
    printf("back %d\n", r == SIGABRT);

    return 0;
}
