/* expect: 2: error: a character constant holds one character; for more, use a string */
int c = '\u00e9';
int main(void) { return c; }
