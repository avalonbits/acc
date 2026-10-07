/* Block statics that start at zero, in functions opt-acc makes from their
 * SSA form: a counter, an array, a struct and a pointer, each kept from
 * one call to the next and each a function's own -- two of the same name
 * in two functions are two. */
struct pos { int line, col; };

static int count(void)
{
    static int n;

    return ++n;
}

static int count_too(void)
{
    static int n;

    n += 10;

    return n;
}

static int histogram(int k)
{
    static unsigned char seen[8];

    seen[k & 7]++;

    return seen[k & 7];
}

static int moved(int dl, int dc)
{
    static struct pos at;

    at.line += dl;
    at.col += dc;

    return at.line * 100 + at.col;
}

static const char *last(const char *s)
{
    static const char *was;
    const char *before = was;

    was = s;

    return before;
}

int main(void)
{
    int ok = 0, i, h = 0;

    for (i = 0; i < 5; i++)
        count();
    ok += count() == 6 && count_too() == 10 && count_too() == 20;
    for (i = 0; i < 20; i++)
        h = histogram(i);
    ok += h == 3 && histogram(3) == 4 && histogram(4) == 3;
    moved(1, 2);
    ok += moved(3, 4) == 406;
    ok += last("a") == 0 && *last("b") == 'a' && *last("c") == 'b';

    return ok == 4 ? 42 : ok;
}
