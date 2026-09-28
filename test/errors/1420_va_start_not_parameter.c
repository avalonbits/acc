/* expect: 6:5: error: va_start needs the function's last named parameter */
#include <stdarg.h>
int f(int n, ...) {
    va_list ap;
    int k = 0;
    va_start(ap, k);
    return n;
}
int main(void) {
    return 0;
}
