/* An int local in memory -- its address taken -- given a long constant:
 * stored at the int's width. The machine IR stored all four bytes, the
 * last over the local after it. */
int x;
static int peek(int *p) { return *p; }
int main(void)
{
    int a = 0;
    int b = -0x7fffffff;
    int *pa = &a, *pb = &b;

    x = peek(pa) + peek(pb);
    return x == 1 ? 42 : 1;
}
