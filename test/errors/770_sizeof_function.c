/* expect: 6: error: 'f' is a function, which has no size */
int f(void) {
    return 1;
}
int main(void) {
    return sizeof f;
}
