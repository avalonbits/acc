/* Where a pointer's const is, when there are parentheses:
 * - `void * const (*p2)[2]`: the const is the elements' -- the stars
 *   beside p2 are its own, and it is an ordinary pointer (c99-restrict-4);
 * - `int * const (p)` and `int * (* const q)`: p and q are const;
 * - `((void *)(v))` with v a const pointer reads v, which a change to what
 *   a parenthesis leaves had refused as changing it (pr52862). */
void *restrict const (*p2)[2];
void *pair[2];
int x;
int *const (p) = &x;
int *(*const q) = 0;

int main(void)
{
    int *const v = &x;
    const int k = 40;
    void *w = ((void *)(v));

    p2 = &pair;
    *p = ((k)) + 2;

    return p2 == &pair && q == 0 && w == &x ? x : 1;
}
