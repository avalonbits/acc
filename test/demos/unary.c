/* Unary minus and ~, which acc folds where it can. Prints 000034 -- 52. */
int main(void) {
    int x = 10;
    int y = -x;
    int z = ~x;         /* -x - 1, so -11 */
    return -y + z + 53;
}
