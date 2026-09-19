/* Character constants and string literals: every escape, adjacent strings
 * joined, strings as pointers and as the initial values of char arrays --
 * sized by the string, with room to spare, filled exactly with no room for
 * the terminator, and as the rows of a 2-D array -- local and global.
 */
char *greeting = "hello";
char name[] = "acc";                    /* four elements, the last a zero */
char exact[3] = "abc";                  /* no room for one, and none added */
char rows[3][4] = {"ab", "xyz"};        /* the third row all zeros */
unsigned char bytes[] = "\x80\377";

int length(char *s) {
    int n = 0;

    while (*s++)
        n++;

    return n;
}

int same(char *a, char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}

int main(void) {
    int r = 0;
    char local[8] = "wor";
    char sized[] = "sized";
    char *joined = "hel" "lo" " " "world";

    /* Every escape C has, each as the value it stands for. */
    if ('\n' == 10 && '\t' == 9 && '\r' == 13 && '\a' == 7 && '\b' == 8
        && '\f' == 12 && '\v' == 11) r = r + 1;
    if ('\\' == 92 && '\'' == 39 && '\"' == 34 && '\?' == 63 && '\0' == 0)
        r = r + 1;
    if ('\101' == 65 && '\x41' == 65 && '\7' == 7 && '\x7f' == 127) r = r + 1;
    /* A char is signed here, so a byte past 127 is negative, as a char. */
    if ('\377' == -1 && '\x80' == -128 && 'A' + 1 == 'B') r = r + 1;

    if (length(greeting) == 5 && same(greeting, "hello")) r = r + 1;
    if (length(joined) == 11 && joined[6] == 'w' && same(joined, "hello world"))
        r = r + 1;
    if (sizeof_name() == 0 && name[3] == 0 && name[2] == 'c') r = r + 1;
    if (exact[2] == 'c' && rows[1][2] == 'z' && rows[0][2] == 0
        && rows[2][3] == 0) r = r + 1;
    if (bytes[0] == 128 && bytes[1] == 255 && bytes[2] == 0) r = r + 1;

    /* A local array copied from its string, the rest zeroed. */
    if (same(local, "wor") && local[3] == 0 && local[7] == 0) r = r + 1;
    if (length(sized) == 5 && sized[5] == 0) r = r + 1;

    /* A string literal subscripted where it stands, and one with every kind
     * of escape inside it. */
    if ("abc"[1] == 'b' && length("a\tb\\c\"d\x41\101") == 9) r = r + 1;

    /* 12 */
    return r + 30;
}

int sizeof_name(void) {
    return 0;
}
