/* String literals inside functions the leaf backend makes: each one's
 * bytes jumped over where it is, and its address -- moved with the code
 * made again -- right wherever it is read. */
static const char *pick(int which)
{
    if (which == 0)
        return "zero";
    if (which == 1)
        return "one";

    return which > 9 ? "many" : "some";
}

static int count_in(const char *s, char c)
{
    int n = 0;

    while (*s)
        if (*s++ == c)
            n++;

    return n;
}

static int vowels(int which)
{
    const char *word = pick(which);
    int n = 0;

    n += count_in(word, 'a') + count_in(word, 'e') + count_in(word, 'i');
    n += count_in(word, 'o') + count_in(word, 'u');

    return n * 10 + "0123456789"[which % 10] - '0';
}

static int same(const char *s, const char *t)
{
    while (*s && *s == *t) {
        s++;
        t++;
    }

    return *s == *t;
}

int main(void)
{
    int right = 0;

    right += same(pick(0), "zero") && same(pick(1), "one");
    right += same(pick(5), "some") && same(pick(12), "many");
    right += vowels(0) == 20 && vowels(1) == 21 && vowels(12) == 12;
    right += "xyz"[2] == 'z' && sizeof "xyz" == 4;
    return right == 4 ? 42 : right;
}
