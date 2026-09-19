/* expect: 8: error: '_Complex' is not supported yet */
/* Every word C99 reserves is interned, so one acc has not implemented is
 * named as itself. Left out, `_Complex` lexes as an ordinary identifier and the
 * complaint is that it is not declared -- which points at the wrong thing
 * entirely, since declaring it would not help. */
int main(void) {
    while (1) {
        return _Complex(int);
    }
    return 0;
}
