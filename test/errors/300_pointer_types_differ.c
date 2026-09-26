/* expect: 9:17: error: these are pointers to different types */
/* Two pointers can only be subtracted or compared when they agree about what
 * they point at, because the difference is in objects and not in bytes. */
int main(void) {
    int n = 1;
    char c = 2;
    int *p = &n;
    char *q = &c;
    return p - q;
}
