/* A macro argument, or an #if's condition, whose expansion runs past 512
 * characters. Both are built in the text buffer #if uses, which was a
 * fixed 512 bytes, and acc stopped with "the expression in an #if is too
 * long" -- on a call that had no #if in it. <agon/vdp/buffer.h>'s SEND of
 * six W()s was one. */
#define W(v) ((v) & 0xff), (((v) >> 8) & 0xff)
#define SUM(...) sum((int[]){__VA_ARGS__}, sizeof((int[]){__VA_ARGS__}) / sizeof(int))
#define TEN(x) (x) + (x) + (x) + (x) + (x) + (x) + (x) + (x) + (x) + (x)
#define LONG_ONE (1 * 1 * 1 * 1 * 1 * 1 * 1 * 1 * 1 * 1 * 1 * 1 * 1 * 1)

static int sum(const int *v, int n)
{
    int s = 0;

    while (n--)
        s += *v++;

    return s;
}

int main(void)
{
    int r = 0;

    /* Twelve bytes from six words, well past 512 characters expanded. */
    if (SUM(W(0x0101), W(0x0202), W(0x0303), W(0x0404), W(0x0505),
            W(0x0606)) == 42)
        r++;

    /* Ten copies of a 50-character macro inside another's argument. */
    if (TEN(TEN(LONG_ONE)) == 100)
        r++;

#if TEN(TEN(LONG_ONE)) == 100
    r++;
#endif

    return r + 39;              /* 3 checks */
}
