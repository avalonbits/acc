/* expect: 4:33: error: 'nope' is not declared */
/* An error in a for loop's step is reported where the step is written. */
int main(void) {
    for (int i = 0; i < 3; i += nope)
        ;
    return 0;
}
