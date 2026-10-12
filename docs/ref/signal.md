# `signal.h`

Signals, which on the Agon are only the ones a program raises itself.

The header gives `sig_atomic_t` (an `int`, 24 bits), the handlers
`SIG_DFL`, `SIG_IGN` and `SIG_ERR`, and the six signals C names, with
the numbers Linux gives them.

Nothing on the Agon sends a program a signal. Escape at the keyboard is
the end of `stdin`'s input, not `SIGINT`; a division by zero or a
floating point error raises no `SIGFPE`; and nothing raises `SIGSEGV`
or `SIGILL`. A signal reaches a handler only when the program calls
`raise`, or `abort`, which raises `SIGABRT`.

## `SIGABRT`, `SIGFPE`, `SIGILL`, `SIGINT`, `SIGSEGV`, `SIGTERM`

    #define SIGINT  2
    #define SIGILL  4
    #define SIGABRT 6
    #define SIGFPE  8
    #define SIGSEGV 11
    #define SIGTERM 15

The signals C names. Any number from 1 to 15 is a signal to `signal` and
`raise`, these six and the nine between them that have no name.

## `signal`, `SIG_DFL`, `SIG_IGN`, `SIG_ERR`

    #define SIG_DFL ((void (*)(int)) 0)
    #define SIG_ERR ((void (*)(int)) -1)
    #define SIG_IGN ((void (*)(int)) 1)

    void (*signal(int sig, void (*func)(int)))(int);

Set what happens when `sig` is raised, and answer what was set before:

- `SIG_DFL`, which every signal starts with, ends the program.
- `SIG_IGN` ignores the signal.
- A function is called with the signal's number, as its handler.

A `sig` that is not 1 to 15 answers `SIG_ERR` and changes nothing.
`errno` is not set.

## `raise`

    int raise(int sig);

Raise `sig` in the program, now, and answer 0 once it is dealt with;
a `sig` that is not 1 to 15 answers -1.

A handler is called with `sig` after the signal has been put back to
`SIG_DFL`, so a handler that raises its own signal ends the program
rather than calling itself for ever. To catch the signal again, the
handler calls `signal` for it once more. When the handler returns,
`raise` returns 0.

With `SIG_DFL`, the program ends with the status 128 and `sig` (134 for
`SIGABRT`), which is what a shell reports for a program a signal ended.
It ends through `_Exit`, so the `atexit` functions do not run and files
are not closed.

    static volatile sig_atomic_t stop;

    static void on_term(int sig)
    {
        stop = 1;
    }

    signal(SIGTERM, on_term);
    ...
    raise(SIGTERM);                 /* on_term runs; stop is 1 */

`abort` raises `SIGABRT` when the program has called `signal`; see
[`abort`](stdlib.md#abort).

## `sig_atomic_t`

    typedef int sig_atomic_t;

The type of a variable a handler sets for the rest of the program to
see, as `stop` above: an `int`, 24 bits. Declare it `volatile`, so
that each read in a loop reads it again.
