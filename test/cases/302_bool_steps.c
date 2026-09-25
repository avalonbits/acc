/* ++ and -- on a _Bool, compared with zero straight after: the step makes
 * the value and a conversion to _Bool takes it back, and the comparison
 * comes after both. A mark that one of them left on the bytes it wrote,
 * read after the other had taken those bytes back and written new ones to
 * the same length, jumped on flags from code that was no longer there. */
int main(void)
{
    _Bool u = 0;
    int r = 0;

    if (u++ != 0) return 1;
    if (u != 1) return 2;
    if (u++ != 1) return 3;
    if (u != 1) return 4;
    u = 0;
    if (++u != 1) return 5;
    if (++u != 1) return 6;
    u = 0;
    if (u-- != 0) return 7;
    if (u != 1) return 8;
    if (u-- != 1) return 9;
    if (u != 0) return 10;
    u = 0;
    if (--u != 1) return 11;
    if (--u != 0) return 12;
    r += (u++ == 0) + (u-- != 0) + !u;

    return r == 3 ? 42 : 13;
}
