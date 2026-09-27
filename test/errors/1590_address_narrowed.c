/* expect: 9:29: error: an address is 3 bytes and this keeps only 1 of them */
/* With a value, so that its address is known as this is compiled and is a
 * constant this could spoil. One with no value is in the bss, whose address
 * is not known until the end: it is loaded whole and relocated, and what the
 * program then does to it is the program's business. */
int g = 1;

int main(void) {
    char c = (char) (int) &g;

    return c;
}
