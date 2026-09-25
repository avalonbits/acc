/* Joined lines, where the file is copied down a run at a time between one
 * backslash and the next: the newlines a join took out go back at the end
 * of its line -- not later, past other backslashes, and not before -- and
 * an escape, an escaped backslash at a line's end, and several joins in one
 * line all keep their bytes. */
static const char *esc = "a\n\
b\\";
static const char *tab = "\t\\\
x";

int main(void)
{
    int r = 0;
    int a\
b\
c = 3;

    if (esc[0] == 'a' && esc[1] == '\n' && esc[2] == 'b' && esc[3] == '\\'
        && esc[4] == 0) r++;
    if (tab[0] == '\t' && tab[1] == '\\' && tab[2] == 'x' && tab[3] == 0) r++;
    if (abc == 3) r++;
    if (__LINE__ == 22) r++;    /* four joins above: each still a line */
    { const char *s = "\"\\"; if (s[0] == '"' && s[1] == '\\') r++; } int x\
= 5;
    if (x == 5 && __LINE__ == 25) r++;

    return r + 36;              /* 6 checks, and 36 */
}
