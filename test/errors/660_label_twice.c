/* expect: 6: error: the label 'here' is defined twice */
/* A label names one place in its function. */
int main(void) {
here:
    ;
here:
    return 0;
}
