/* expect: 5:5: error: va_start in a function with no '...' */
#include <stdarg.h>
int f(int n) {
    va_list ap;
    va_start(ap, n);
    return n;
}
int main(void) {
    return 0;
}
