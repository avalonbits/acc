/* expect: 6:5: error: 'total' is already a variable */
/* A function may not take a name a global already has.
 */
int total;

int total(void) {
    return 1;
}

int main(void) {
    return total;
}
