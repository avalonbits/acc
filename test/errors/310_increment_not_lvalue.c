/* expect: 6:14: error: '++' needs a variable, or something a pointer points at, and this is a number */
/* The operand is checked where it is parsed, rather than left to produce a
 * value and then find nowhere to put the changed one back. */
int main(void) {
    int a = 1;
    return ++3 + a;
}
