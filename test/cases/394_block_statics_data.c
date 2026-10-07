/* Block statics with initial values, in functions opt-acc makes from
 * their SSA form: their bytes laid down where the code starts and jumped
 * over, as the first pass laid them where they were declared. An int
 * table, a string, bytes changed as the program runs, a struct, and a
 * counter that starts other than at zero. */
struct span { int from, to; };

static int table(int k)
{
    static const int t[4] = { 5, 6, 7, 1000000 };

    return t[k];
}

static char letters(int k)
{
    static const char s[] = "hello";
    static unsigned char bumps[3] = { 1, 2, 250 };

    bumps[k % 3]++;

    return (char) (s[k] + bumps[k % 3]);
}

static int width(void)
{
    static struct span sp = { 10, 25 };

    sp.to++;

    return sp.to - sp.from;
}

static int ticket(void)
{
    static int next = 100;

    return next++;
}

int main(void)
{
    int ok = 0;

    ok += table(0) == 5 && table(2) == 7 && table(3) == 1000000;
    ok += letters(1) == 'e' + 3 && letters(1) == 'e' + 4
          && letters(2) == (char) ('l' + 251);
    ok += width() == 16 && width() == 17;
    ok += ticket() == 100 && ticket() == 101 && ticket() == 102;

    return ok == 4 ? 42 : ok;
}
