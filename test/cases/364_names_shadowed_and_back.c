/* A name a local has, and has again in a block inside, means each in turn
 * and the outer one again when the inner block ends; a file-scope name
 * comes back when the function's locals are gone. And a file-scope symbol
 * made while a local has its name -- `extern` in a block, for a name an
 * outer block's local has and no file-scope declaration has had yet -- is
 * the one the name means at file scope after; as is a function declared
 * first in a block, whose symbol moves the locals along under the names
 * that point at them. */
int x = 7;
int counter_seen(void);

int shadows(void)
{
    int r = 0;
    int x = 1;

    r += x == 1;
    {
        int x = 2;

        r += x == 2;
        {
            int x = 3;

            r += x == 3;
        }
        r += x == 2;
    }
    r += x == 1;

    return r;
}

int outer_local(void)
{
    int counter = 5;
    int r = 0;

    {
        extern int counter;

        counter = 40;
        r += counter == 40;
    }
    r += counter == 5;

    return r;
}

int counter;

int called_late(void)
{
    int before = 11, r = 0;

    {
        int inner = 12;
        int later(int);

        r += later(inner) == 13;
        r += inner == 12 && before == 11;
    }
    r += before == 11;

    return r;
}

int later(int n)
{
    return n + 1;
}

int counter_seen(void)
{
    return counter;
}

int main(void)
{
    int r = shadows() + outer_local() + called_late();

    r += x == 7;
    r += counter_seen() == 40;

    return r == 12 ? 42 : r;
}
