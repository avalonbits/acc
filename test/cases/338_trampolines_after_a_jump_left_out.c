/* `if (a && b) o = f(o);` with o changed before it: both branches to the
 * join carry a copy of o to where the join keeps it, each in a trampoline
 * of its own. The block after the second test is only a jump over a block
 * nothing reaches, which opt-acc's own backend leaves out, falling into
 * the body instead -- and it laid the trampolines down there, so a true
 * test fell into the first of them and skipped the body. zap lost the
 * address of every `call.lis`. */
static char *emit(char *o, int x)
{
    o[0] = (char) x;
    o[1] = (char) (x + 1);

    return o + 2;
}

/* `o` is read after the join, so `end` cannot share its home: each
 * branch to the join copies o into end's. */
static int row(char *o, const unsigned char *mode, const unsigned char *cond,
               int x)
{
    char *end;

    *o++ = 9;
    end = o;
    if ((*mode & 2) && (*cond & 0x30))
        end = emit(o, x);

    return (int) (end - o);
}

int main(void)
{
    char buf[8];
    unsigned char two = 2, zero = 0, both = 0x30;
    int right = 0;

    right += row(buf, &two, &both, 5) == 2 && buf[1] == 5;
    right += row(buf, &two, &zero, 5) == 0;
    right += row(buf, &zero, &both, 5) == 0;
    return right == 3 ? 42 : right;
}
