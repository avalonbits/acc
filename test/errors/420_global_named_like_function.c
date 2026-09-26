/* expect: 8:5: error: 'f' is already a function */
/* Nor may a global take a function's.
 */
int f(void) {
    return 1;
}

int f;

int main(void) {
    return f();
}
