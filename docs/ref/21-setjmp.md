# `setjmp.h`

Jumps from a function back to one that called it, past the returns in
between.

The header gives `jmp_buf`. `setjmp` and `longjmp` are in acc's runtime,
written in eZ80 assembly, since they put back the stack pointer and the
frame pointer, which C cannot name.

## `setjmp`, `longjmp`, `jmp_buf`

    typedef int jmp_buf[4];

    int  setjmp(jmp_buf env);
    void longjmp(jmp_buf env, int val);

`setjmp` saves in `env` the place it was called from and answers 0.
`longjmp` goes back there: `setjmp` returns a second time, answering
`val`, or 1 when `val` is 0, so that the second return is never taken
for the first. `longjmp` does not return.

    static jmp_buf on_error;

    if (setjmp(on_error)) {
        puts("bad input");
        return 1;
    }
    parse(text);            /* calls longjmp(on_error, 1) to give up */

A `jmp_buf` is four words, 12 bytes: the return address of the call to
`setjmp`, IX, the stack pointer as it was at the call, and IY. That is
all a call keeps across it in registers. The function's other locals
live in its frame, where IX points, so after `longjmp` they hold
whatever values they had when `longjmp` was called. IY is the one
local acc can keep in a register; it is saved and put back, so a
local held there is as it was when `setjmp` was called. C says the
value of a local changed between the two is uncertain unless it is
`volatile`, and a program that has to be right on other compilers too
declares those locals `volatile`.

acc knows `setjmp` by its name, and compiles a call to it with no value
kept in registers around it: call it by its name, not through a pointer.
The function that called `setjmp` must still be running when `longjmp`
is called, since `longjmp` goes back into its frame.
