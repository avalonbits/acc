/* expect: 5: error: 'f' is already a function */
/* A variable and a function share the one name space at file scope. Two
 * declarations of one variable are fine, and 930_extern has them. */
int f(void) { return 1; }
int f;

int main(void) {
    return 0;
}
