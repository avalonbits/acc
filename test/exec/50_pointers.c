/* expect:
g 42
arr 10 20 30
sum 60
swap 5 3
str hello
*/
int printf(const char *, ...);
int g = 42;
int arr[3];
void swap(int *a, int *b){ int t = *a; *a = *b; *b = t; }
int sum(int *p, int n){ int s = 0; while (n > 0) { s = s + *p; p = p + 1; n = n - 1; } return s; }
int main(void) {
    int x = 3, y = 5;
    int *p = &g;
    printf("g %d\r\n", *p);
    arr[0] = 10; arr[1] = 20; arr[2] = 30;
    printf("arr %d %d %d\r\n", arr[0], arr[1], arr[2]);
    printf("sum %d\r\n", sum(arr, 3));
    swap(&x, &y);
    printf("swap %d %d\r\n", x, y);
    printf("str %s\r\n", "hello");
    return 0;
}
