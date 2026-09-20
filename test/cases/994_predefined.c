/* __FILE__ and __LINE__, against agondev compiling the same program.
 *
 * __FILE__ is the path the compiler was given, which the two are not run
 * with in the same form, so what is compared here is what can be: that it
 * is a string, that it is not empty, and that it ends in ".c". __LINE__ is
 * the same number for both, and is what most of this checks.
 */
int first_line(void) { return __LINE__; }   /* line 8 */

int after_blank(void) {
    return __LINE__;                        /* line 11 */
}

#define WHERE __LINE__

int through_a_macro(void) { return WHERE; } /* line 16 */

static int ends_in_dot_c(const char *s) {
    int n = 0;

    while (s[n])
        n++;

    return n > 2 && s[n - 2] == '.' && s[n - 1] == 'c';
}

int main(void) {
    const char *file = __FILE__;

    if (!ends_in_dot_c(file))
        return 1;

    /* 8 + 11 + 16 = 35 */
    return first_line() + after_blank() + through_a_macro() + 7;
}
