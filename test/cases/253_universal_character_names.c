/* Universal character names, C99 6.4.3, in names, strings and constants.
 *
 * \u00e9, \U000000e9 and the UTF-8 for it typed as it is are one name: the variable
 * below is declared with the first, used with the second, and used again
 * with the raw bytes. In a string a universal character name is the
 * character's UTF-8, as agondev has it, so "\u00e9" is two bytes and a
 * terminator. A backslash that is itself escaped does not begin one:
 * "\\u00e9" is seven bytes.
 *
 * The bytes of this file matter: every universal character name in it is
 * written as the six or ten characters it is, and exactly one line spells
 * the name as raw UTF-8. */
#define \u00e9t\u00e9 2

/* \u0041 names a character C forbids one to name, and in a comment it is
 * nothing at all, since comments go before names mean anything. */

static int caf\u00e9 = 40;

int main(void) {
    int r = 0;
    const unsigned char *e = (const unsigned char *) "\u00e9";
    const unsigned char *g = (const unsigned char *) "\U0001F600";

    if (caf\U000000e9 + \u00e9t\U000000e9 == 42) r++;
    if (café == 40) r++;                     /* the raw UTF-8 spelling */
    if (sizeof ("\u00e9") == 3 && e[0] == 0xc3 && e[1] == 0xa9) r++;
    if (sizeof ("\U0001F600") == 5 && g[0] == 0xf0 && g[1] == 0x9f
        && g[2] == 0x98 && g[3] == 0x80) r++;
    if (sizeof ("\\u00e9") == 7 && "\\u00e9"[1] == 'u') r++;
    if ('\u0040' == '@' && '\u0024' == '$' && "\u0060"[0] == '`') r++;

    return r + 36;              /* 6 checks */
}
