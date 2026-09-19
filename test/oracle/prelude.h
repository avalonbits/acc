/* What acc builds in and agondev takes from a header: included ahead of
 * every case for the reference build only.
 *
 * acc has no preprocessor, so a program for it cannot include anything,
 * and what <stdarg.h> gives -- va_list, va_start, va_arg, va_end, va_copy --
 * acc has as words of its own. agondev has them from the header. */
#include <stdarg.h>
