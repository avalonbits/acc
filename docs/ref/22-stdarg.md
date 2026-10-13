# `stdarg.h`

Reading the arguments a function declared with `...` was given past its
named parameters.

The header gives the type `va_list` and the four macros C99 names. They
are spelled over acc's own words for them, `__va_list`, `__va_start`,
`__va_arg`, `__va_end` and `__va_copy`, which are part of the language
rather than macros written in C, since `va_arg` has to know how wide a
value is to step over it. A program that does not include the header
may use `va_list` and the rest as names of its own, and one that does
can ask `#ifdef va_copy`.

## `va_list`, `va_start`, `va_arg`, `va_end`

    typedef __va_list va_list;

    void va_start(va_list ap, last);
    type va_arg(va_list ap, type);
    void va_end(va_list ap);

A `va_list` is a `char *` that walks along the arguments on the stack.
`va_start` points it just past `last`, the function's last named
parameter. Each `va_arg` then reads a value of `type` where it points,
and steps it past the room that value took; `va_end` has nothing to
undo, and does nothing.

    int sum(int count, ...)
    {
        va_list ap;
        int total = 0;

        va_start(ap, count);
        while (count-- > 0)
            total += va_arg(ap, int);
        va_end(ap);

        return total;
    }

Every argument fills whole three-byte slots, as the call pushed it; a
`struct` is rounded up to whole slots:

| Argument                           | Room              |
|------------------------------------|-------------------|
| `char`, `short`, `int`, a pointer  | one slot, 3 bytes |
| `long`, `float`, `double`          | two, 6 bytes      |
| `long long`                        | three, 9 bytes    |
| a `struct` or `union`              | its size, rounded |

A `char` or a `short` argument is passed as an `int`, so read it with
`va_arg(ap, int)`. `float` and `double` are the same four-byte type on
acc, so `va_arg(ap, double)` reads a `float` argument as well.

`va_arg` reads what it is told to: nothing checks the type against the
argument, or that there is an argument left to read. The function must
know from its named parameters how many there are and what they are, as
`printf` knows from its format.

acc refuses `va_start` in a function that has no `...`, and a `last`
that is not one of the function's parameters. It refuses a `va_arg` of
a function type, and any `ap` that is not a `va_list`. `ap` may be any
expression that names one, such as `*p` for a `va_list *p` or an
element of an array of them, and the value of `va_arg` may be
subscripted or called like any other.

## `va_copy`

    void va_copy(va_list to, va_list from);

Make `to` a copy of `from`, at the same place among the arguments, so
that the arguments can be read twice. It is an assignment, `to = from`.

    va_list again;

    va_copy(again, ap);
    n = vsnprintf(NULL, 0, fmt, again);     /* measure */
    va_end(again);
    vsprintf(buf, fmt, ap);                 /* then write */

A `va_list` handed to a function, such as
[`vprintf`](04-stdio.md#vprintf-vfprintf-vsprintf-vsnprintf), is a copy
of the pointer: what the function reads with it does not move the
caller's.
