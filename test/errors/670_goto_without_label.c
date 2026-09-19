/* expect: 4: error: 'goto' needs a label, and this is a number */
/* goto takes a name, not a computed address. */
int main(void) {
    goto 3;
    return 0;
}
