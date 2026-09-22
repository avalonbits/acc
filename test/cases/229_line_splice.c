/* A backslash and the newline after it, which C deletes before anything is
 * tokenised.
 *
 * Inside a directive that has always worked here, because a macro's line is
 * gathered before it is read. Outside one it did not: the lexer reads the
 * file where it sits, and a backslash it did not expect was a stray
 * character. What C says is that the two go before the file is a sequence
 * of tokens at all, so a name may be split over two lines, a string may run
 * over as many as it likes, and a join between two tokens leaves nothing
 * behind at all.
 */
static const char *two_lines = "abc\
def";

static int spl\
it_name(int n) { return n + 1; }

int main(void) {
    int r = 0;
    int val\
ue = 20;

    /* Between tokens, where the join leaves nothing. */
    r = r + \
        value;

    if (two_lines[3] == 'd' && two_lines[6] == 0) r = r + 1;
    if (split_name(20) == 21) r = r + 1;
    if (value == 20) r = r + 1;

    /* A join inside a string keeps the two halves one string, so its
     * length is the two together. */
    {
        int n = 0;

        while (two_lines[n])
            n++;
        if (n == 6) r = r + 1;
    }

    /* The lines a join took out are still counted: three joins are above
     * this, and what is under them is on the line it was written on. A
     * count that lost them would put every diagnostic after a joined line
     * on the wrong one. */
    if (__LINE__ == 45) r = r + 1;

    return r + 17;              /* 20 + 5 checks, and 17 */
}
