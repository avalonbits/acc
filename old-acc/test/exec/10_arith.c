/* expect:
28
-4
96
4
2
-2
*/
int printf(const char *, ...);
int main(void) {
    int a = 12, b = 16;
    printf("%d\r\n", a + b);
    printf("%d\r\n", a - b);
    printf("%d\r\n", a * 8);
    printf("%d\r\n", b / 4);
    printf("%d\r\n", b % 7);
    printf("%d\r\n", -a / 6);
    return 0;
}
