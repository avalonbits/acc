/* expect: 8:25: error: the two sides of ?: are pointers to different types */
/* The two sides of `?:` have to meet at one type, and two pointers only do
 * when they agree about what they point at. */
int main(void) {
    int a = 1;
    char b = 2;
    int c = 1;
    int *p = c ? &a : &b;
    return *p;
}
