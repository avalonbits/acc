/* expect: 6:7: error: the left of '=' is not something that can be assigned to */
/* An array is not something C can assign to as a whole. */
int main(void) {
    int a[2];
    int b[2];
    a = b;
    return 0;
}
