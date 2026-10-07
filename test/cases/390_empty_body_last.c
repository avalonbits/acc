static int calls;

static void bar(void)
{
}

static void doit(int x, int y)
{
    calls++;
    if (x < y)
        return;
    bar();
}

int main(void)
{
    doit(1, 2);
    doit(2, 1);

    return calls == 2 ? 42 : 1;
}
