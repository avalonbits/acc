/* expect: 4:5: error: va_start in a function with no '...' */
int f(int n) {
    va_list ap;
    va_start(ap, n);
    return n;
}
int main(void) {
    return 0;
}
