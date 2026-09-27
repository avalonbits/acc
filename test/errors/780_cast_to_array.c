/* expect: 5:14: error: a cast cannot make an array */
/* A value is never an array, so no conversion can produce one. */
int main(void) {
    int a[2];
    return ((int[2]) a)[0];
}
