/* With <stdarg.h> included, va_start, va_arg, va_end and va_copy are
 * macros, as C99 7.15.1 has them -- so the portable fallback for a library
 * without va_copy, `#ifndef va_copy`, finds acc's -- and va_list a type.
 */
#include <stdarg.h>

static int sum(int n, ...)
{
    va_list ap, again;
    int s = 0, i;

    va_start(ap, n);
    va_copy(again, ap);
    for (i = 0; i < n; i++)
        s += va_arg(ap, int);
    for (i = 0; i < n; i++)
        s += va_arg(again, int);
    va_end(again);
    va_end(ap);

    return s;
}

int main(void) {
    int r = 0;

#if defined(va_start) && defined(va_arg) && defined(va_end)
    r++;
#endif
#ifdef va_copy
    r++;
#endif
    return r + 20 + sum(4, 1, 2, 3, 4);         /* 2 + 20 + 20 */
}
