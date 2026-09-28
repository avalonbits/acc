/* Variable arguments: `...` in a declaration and a definition, calls with
 * any number past the named ones, and va_start, va_arg, va_end and va_copy
 * reading ints, longs, chars (which come as ints), pointers and structs --
 * and a va_list handed to another function. */
#include <stdarg.h>

struct pair { int a; char b; };

int sum(int n, ...);

int vsum(int n, va_list ap) {
    int s = 0;

    while (n--)
        s += va_arg(ap, int);

    return s;
}

int sum(int n, ...) {
    va_list ap;
    int s;

    va_start(ap, n);
    s = vsum(n, ap);
    va_end(ap);

    return s;
}

long mixed(const char *kinds, ...) {
    va_list ap, again;
    long total = 0;

    va_start(ap, kinds);
    va_copy(again, ap);
    for (; *kinds; kinds++) {
        switch (*kinds) {
        case 'i': total += va_arg(ap, int); break;
        case 'l': total += va_arg(ap, long); break;
        case 'c': total += (char) va_arg(ap, int); break;
        case 's': total += *va_arg(ap, char *); break;
        case 'p': {
            struct pair p = va_arg(ap, struct pair);

            total += p.a + p.b;
            break;
        }
        }
    }
    total += va_arg(again, int);        /* the first one, read again */
    va_end(again);
    va_end(ap);

    return total;
}

int count(int n, ...) {
    return n;
}

int main(void) {
    int r = 0;
    struct pair p = { 100, 7 };
    int (*fp)(int, ...) = sum;

    if (sum(3, 1, 2, 3) == 6 && sum(0) == 0) r++;
    if (mixed("il", 5, 100000L) == 100005 + 5) r++;
    if (mixed("csp", 'a', "b", p, 9) == 'a' + 'b' + 107 + 'a') r++;
    if (mixed("lil", 70000L, 2, 30000L) == 100002 + 70000L % 16777216 * 0 + 70000) r++;
    if (count(4, 1L, "x", p) == 4 && fp(2, 20, 22) == 42) r++;

    return r + 37;          /* 5 checks */
}
