/* expect: 9:31: error: an address cannot become a floating-point value */
/* With a value, so that its address is known as this is compiled and is a
 * constant this could spoil. One with no value is in the bss, whose address
 * is not known until the end: it is loaded whole and relocated, and what the
 * program then does to it is the program's business. */
int g = 1;

int main(void) {
    float f = (float) (int) &g;

    return (int) f;
}
