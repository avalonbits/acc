/* expect:
4144
65535
58052
240
15
-16
4095
*/
int printf(const char *, ...);
int main(void) {
    int a = 0x1234, b = 0xF0F0;
    printf("%d\r\n", a & b);
    printf("%d\r\n", 0xFF00 | 0x00FF);
    printf("%d\r\n", a ^ b);
    printf("%d\r\n", 0x0F << 4);
    printf("%d\r\n", 0xF0 >> 4);
    printf("%d\r\n", -256 >> 4);
    printf("%d\r\n", ~(-4096));
    return 0;
}
