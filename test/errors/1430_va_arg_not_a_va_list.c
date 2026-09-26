/* expect: 5:19: error: this has to be a va_list, and it is not */
int f(int n, ...) {
    int notalist = 0;

    return va_arg(notalist, int);
}
int main(void) { return f(1, 2); }
