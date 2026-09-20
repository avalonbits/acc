/* expect: 7: error: only adding a number to an address, or taking one from it, gives something that still moves with the program, and this does not */
/* The answer would be a number that depends on where the program was loaded,
 * which no relocation could put right. Said at run time it still works: the
 * address into a variable first, the masking after. */
int g;

int masked = (int) &g & 255;
