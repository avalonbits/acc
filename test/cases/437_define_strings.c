/* What opens a comment, inside a string or a character constant in a
 * define, is part of it: a URL in a help text, a quote escaped before a
 * slash, and the two slashes as characters. A comment after them on the
 * same line is still a comment, and so is one in a line joined on. */
#define URL    "see <https://example.org/a//b>, /* kept */" // gone
#define ESC    "q\"//x"
#define SLASH  '/', '/'
#define JOINED "a//b" \
               "c/*d*/" /* gone too */

static int same(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}

int main(void)
{
    static const char slashes[] = { SLASH, 0 };

    if (!same(URL, "see <https://example.org/a//b>, /* kept */"))
        return 1;
    if (sizeof ESC != 6 || ESC[1] != '"' || ESC[2] != '/')
        return 2;
    if (!same(slashes, "//"))
        return 3;
    if (!same(JOINED, "a//bc/*d*/"))
        return 4;

    return 42;
}
