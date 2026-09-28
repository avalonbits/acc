/* expect: 4:9: error: va_arg reads a value, and a function type has none */
#include <stdarg.h>
int f(int n, ...) { va_list ap; int e; va_start(ap, n);
    e = va_arg(ap, int (void))();
    va_end(ap); return e; }
int main(void) { return 0; }
