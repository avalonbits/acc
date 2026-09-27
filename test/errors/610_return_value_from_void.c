/* expect: 4:5: error: this function returns void, so 'return' cannot give it a value */
/* C99 allows a return with a value only where there is one to give. */
void f(void) {
    return 3;
}

int main(void) {
    f();
    return 0;
}
