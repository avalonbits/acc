/* expect: 4:10: error: this string is longer than the array it initialises */
/* Four characters do not fit in three. */
int main(void) {
    char s[3] = "abcd";
    return s[0];
}
