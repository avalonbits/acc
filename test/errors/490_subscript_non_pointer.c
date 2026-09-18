/* expect: 5: error: '[' needs an array or a pointer, and this is an integer */
/* Only something that points at elements can be subscripted. */
int main(void) {
    int x = 3;
    return x[1];
}
