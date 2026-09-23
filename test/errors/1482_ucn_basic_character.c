/* expect: 2: error: \u0041 is not a character a universal character name may name */
const char *s = "\u0041";
int main(void) { return s[0]; }
