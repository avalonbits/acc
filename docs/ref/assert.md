# `assert.h`

The `assert` macro, which stops a program when something it takes for
granted is not so, and `NDEBUG`, which turns it off.

## `assert`

    void assert(scalar e);

When `e` is 0, print what failed to `stderr` and end the program;
otherwise do nothing. `e` is evaluated once.

The message names the file, the line, the function and the expression
as it was written, in the words glibc uses:

    main.c:12: parse: Assertion `n < MAX' failed.

`assert` then ends the program through `abort`, so its status is 134,
the `atexit` functions do not run, and files are not closed. A program
that set a handler for `SIGABRT` with `signal` has it called first.

The message and the call are a function of the library's own, which
brings `fprintf` with it into the program. `stderr` is not buffered, so
the message is on the screen before the program ends.

## `NDEBUG`

    #define NDEBUG

When `NDEBUG` is defined where `assert.h` is included, `assert(e)` is
`((void) 0)`: `e` is not evaluated, and the program carries neither the
message nor `fprintf`. Define it in front of the `#include`, or with
`-D NDEBUG` on acc's command line.

The header has no include guard round the macro: each time it is
included, `assert` is defined again by whether `NDEBUG` is defined
then, so one file can have it on in one part and off in another.
