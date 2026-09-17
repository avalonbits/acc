/* Comments, in the places they turn up rather than only at the top.
 *
 * The lexer skips them in skip_space, which also counts the newlines inside
 * one so that a later error still names the right line. Nothing else in the
 * suite has a comment anywhere but the first line, so nothing else would
 * notice if a block comment swallowed the token after it.
 */
int add(int a, /* between parameters */ int b) {
    return a /* mid-expression */ + b;   // and to the end of the line
}

/* Spanning
   several
   lines. */
int main(void) {
    int x = 40;   // forty
    /**/          /* empty, and two of them adjacent */

    /* A comment as the last thing before a token that matters. */
    return add(x, 2);
}
