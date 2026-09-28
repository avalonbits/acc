/* va_list, va_start, va_arg, va_end and va_copy are <stdarg.h>'s: C99
 * 7.1.3 reserves a header's names only where it is included, so a program
 * that does not include it may use them for anything. acc's own words for
 * reading variable arguments are spelled __va_list and the rest.
 */
typedef int va_list;

static int va_start(int a) { return a + 1; }

int main(void) {
    va_list va_arg = 20;
    int va_end = 10, va_copy = va_start(10);

    return va_arg + va_end + va_copy + 1;       /* 20 + 10 + 11 + 1 */
}
