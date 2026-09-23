/* A backslash that is escaped does not begin a universal character name,
 * with nothing before it in the file that is one.
 *
 * Names are made UTF-8 by one pass over each window of the source, which
 * searches for the first backslash that needs anything and then walks the
 * rest. The string below is a backslash and then text. The case for
 * universal character names has its escaped backslash after one that is
 * not, so it only ever reaches the walk -- and the search stepped over an
 * escaped backslash with a rule of its own that nothing tested. So this file
 * has no backslash at all before that string, this comment included, and
 * one universal character name after it. */
int main(void) {
    int r = 0;
    const char *s = "\\u00e9";
    const unsigned char *e = (const unsigned char *) "\u00e9";

    if (sizeof ("\\u00e9") == 7 && s[0] == '\\' && s[1] == 'u') r++;
    if (e[0] == 0xc3 && e[1] == 0xa9 && e[2] == 0) r++;

    return r + 40;              /* 2 checks */
}
