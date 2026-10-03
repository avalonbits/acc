/* A value set late in a loop and read after it -- `end`, set where the
 * body goes round and read after the break -- lives through the whole of
 * the loop, the blocks before where it is set as much as after: carried
 * round the jump back. opt-acc's machine-level backend stretched it only
 * from where it was set to where it was read, in the order the blocks are
 * laid out, and gave its register to the body. As gcc's pr34415 has it. */
const char *scan(const char *p)
{
    const char *end;
    int len = 1;

    for (;;) {
        int c = *p;

        c = c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
        if (c == 'B') {
            end = p;
        } else if (c == 'A') {
            end = p;
            do
                p++;
            while (*p == '+');
        } else {
            break;
        }
        p++;
        len++;
    }
    if (len > 2 && *p == ':')
        p = end;

    return p;
}

int main(void)
{
    const char *input = "Bbb:", *other = "aA+b:";
    int r = 0;

    r += scan(input) == input + 2;
    r += scan(other) == other + 2;
    r += scan("x") != 0;

    return r == 3 ? 42 : r;
}
