/* expect: 4:5: error: this function returns a value, so 'return' needs one */
/* And a return without one only where there is none. */
int f(void) {
    return;
}

int main(void) {
    return f();
}
