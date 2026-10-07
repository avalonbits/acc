/* A function whose last statement is `if (...) { ...; return; }`, with a
 * frame: the jump over the if landed past the return, which, taken back
 * into the epilogue, put it on the ret -- IX and SP left as the body had
 * them, and the caller's frame gone. ez80asm's parseOptions returned to
 * nowhere. */
static int said;

static void say(int n)
{
    said += n;
}

static void scan(int argc, int count)
{
    int seen[4] = { 0, 0, 0, 0 };

    seen[argc & 3] = count;
    if ((argc == 1) || (seen[argc & 3] == 0)) {
        say(1);
        return;
    }
}

int main(void)
{
    scan(4, 2);
    scan(1, 2);

    return said == 1 ? 42 : 1;
}
