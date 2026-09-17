/* expect:
sum 45
even 20
fib 55
sw 1 2 3 9
goto 7
*/
int printf(const char *, ...);
int fib(int n){ if (n < 2) return n; return fib(n-1) + fib(n-2); }
int sw(int x){ switch(x){ case 1: return 1; case 2: return 2; case 3: return 3; default: return 9; } }
int main(void) {
    int i, s = 0, e = 0;
    for (i = 0; i < 10; i++) s = s + i;
    printf("sum %d\r\n", s);
    i = 0;
    do { if (i % 2 == 0) e = e + i; i = i + 1; } while (i < 10);
    printf("even %d\r\n", e);
    printf("fib %d\r\n", fib(10));
    printf("sw %d %d %d %d\r\n", sw(1), sw(2), sw(3), sw(7));
    i = 0;
again:
    i = i + 7;
    if (i < 7) goto again;
    printf("goto %d\r\n", i);
    return 0;
}
