/* expect: 2:9: error: a character constant holds one character; for more, use a string */
int c = '\u00e9';
int main(void) { return c; }
