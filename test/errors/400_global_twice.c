/* expect: 5: error: 'x' is already declared, and a second declaration of a global is not supported yet */
/* C would take this, as two tentative definitions; acc takes one.
 */
int x;
int x;

int main(void) {
    return x;
}
