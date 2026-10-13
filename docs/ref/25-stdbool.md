# `stdbool.h`

The boolean type and its two values, by name. The header is four macros
and nothing else.

## `bool`

    #define bool _Bool

`_Bool` is the compiler's own type, one byte, that holds only 0 or 1:
any value stored in it that is not 0 (an integer, a float or a pointer)
is stored as 1.

    bool found = strchr(s, ':');        /* 1 when there is a ':' */

## `true`, `false`

    #define true  1
    #define false 0

Plain `int` constants, as C99 has them, so they can be used in `#if`.

## `__bool_true_false_are_defined`

    #define __bool_true_false_are_defined 1

Says that the three names above are defined.
