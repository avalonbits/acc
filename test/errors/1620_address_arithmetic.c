/* expect: 11: error: only adding a number to an address, or taking one from it, gives something that still moves with the program, and this does not */
/* The answer would be a number that depends on where the program was loaded,
 * which no relocation could put right. Said at run time it still works: the
 * address into a variable first, the masking after. */
/* With a value, so that its address is known as this is compiled and is a
 * constant this could spoil. One with no value is in the bss, whose address
 * is not known until the end: it is loaded whole and relocated, and what the
 * program then does to it is the program's business. */
int g = 1;

int masked = (int) &g & 255;
