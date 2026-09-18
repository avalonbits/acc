/* expect: 9: error: '&&' is not supported yet */
/* `&&` has to be lexed although it is not implemented, because otherwise it
 * is not refused, it is misread: `a && b` becomes a bitwise and of a with the
 * address of b, and what acc said about it was that pointers are not
 * supported. */
int main(void) {
    int a = 1;
    int b = 2;
    return a && b;
}
