/* A function's strings, laid down together where its code starts behind
 * one jump in opt-acc's SSA form, rather than each jumped over where it is
 * read: read in branches, in a loop, the same text twice, and passed on. */
#include <string.h>

static const char *pick(int k)
{
    if (k == 0)
        return "zero";
    if (k == 1)
        return "one";

    return k < 0 ? "negative" : "many";
}

static int count(const char *s, char c)
{
    int n = 0;

    for (; *s; s++)
        n += *s == c;

    return n;
}

static int letters(void)
{
    int i, total = 0;

    for (i = 0; i < 3; i++)
        total += count(i & 1 ? "banana" : "cabana", 'a') + (int) strlen("xyz");

    return total;
}

int main(void)
{
    int ok = 0;

    ok += !strcmp(pick(0), "zero") && !strcmp(pick(1), "one");
    ok += !strcmp(pick(-4), "negative") && !strcmp(pick(9), "many");
    ok += letters() == 3 * 3 + 3 * 3;
    ok += pick(2) == pick(3);

    return ok == 4 ? 42 : ok;
}
