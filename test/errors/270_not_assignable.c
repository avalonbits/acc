/* expect: 6:11: error: the left of '+=' is not something that can be assigned to */
/* What is left over after an expression is checked for an assignment operator,
 * which is how `a + b += 1` gets this rather than "expected ';'". */
int main(void) {
    int a = 1;
    a + 1 += 2;
    return a;
}
