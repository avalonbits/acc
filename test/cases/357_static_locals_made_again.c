/* A block's statics in functions made from the SSA form: their bytes made
 * again where the function's code starts, and every read and write of them
 * at the place they moved to -- a table, a counter kept between calls, two
 * in one function, and one read in a loop. */
static int letter(int i)
{
    static const char letters[] = "etaoinshrdlu";

    return letters[i % 12];
}

static int counter(void)
{
    static int calls = 40;

    return ++calls;
}

static int two(int i)
{
    static const unsigned char small[4] = { 1, 2, 3, 4 };
    static const int big[3] = { 1000, -2000, 3000 };

    return small[i & 3] + big[i % 3];
}

static int vowels(const char *s)
{
    static const char vowel[] = "aeiou";
    int n = 0;

    for (; *s; s++)
        for (int k = 0; vowel[k]; k++)
            if (*s == vowel[k])
                n++;

    return n;
}

int main(void)
{
    int right = 0;

    right += letter(0) == 'e' && letter(13) == 't' && letter(11) == 'u';
    counter();
    right += counter() == 42;
    right += two(0) == 1001 && two(5) == 3002 && two(7) == -1996;
    right += vowels("static locals made again") == 9;
    return right == 4 ? 42 : right;
}
