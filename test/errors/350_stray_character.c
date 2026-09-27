/* expect: 8:11: error: stray '@' in the source */
/* A character that begins no token, reported at its own line: after a
 * comment spanning lines, so a count that the comment or the reporting
 * lost track of would show.
 */
int main(void) {
    int a = 1;
    a = a @ 2;
    return a;
}
