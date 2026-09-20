/* expect: 5: error: an array's length has to be an integer */
/* A length worked out as the program runs is C99's, and acc has it; what
 * it still has to be is an integer. */
int main(void) {
    int a[1.5];
    return a[0];
}
