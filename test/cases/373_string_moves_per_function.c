/* A string literal's address is where the first pass put it until opt-acc
 * lays the function down again; then the string has moved, and each
 * address constant inside it is looked up among the moves. Those have to
 * be the function's own. The first pass's code for pick is longer than
 * what replaces it, so its last string's old place covers where `where`
 * is laid down after it -- and read_it, reading `where` by its address,
 * read somewhere else while the moves made for pick were still there. */
static const char *pick(int k)
{
    if (k == 1) return "one";
    if (k == 2) return "a string with its first-pass bytes past the end";
    return 0;
}

int value = 40;
int *where = &value;

static int read_it(void) { return *where; }

int main(void)
{
    return read_it() + (pick(1)[0] == 'o') + (pick(3) == 0);
}
