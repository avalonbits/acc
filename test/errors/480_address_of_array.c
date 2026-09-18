/* expect: 6: error: the address of a whole array is not supported yet; &a[0] is the address of its first element */
/* &a is a pointer to the whole array, a type acc cannot say yet. */
int main(void) {
    int a[3];
    int *p;
    p = &a;
    return 0;
}
