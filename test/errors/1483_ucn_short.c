/* expect: 2: error: '\u' is a universal character name, and needs 4 hex digits after it */
const char *s = "\u12";
int main(void) { return s[0]; }
