/* An array of unknown size is a type (C99 6.7.5.2p4): a pointer can point
 * at one, a typedef can name one, and `&` of an array not yet given its
 * size is a pointer to one. acc refused all three -- 16 of gcc.dg's tests
 * point at one, and 4 name one in a typedef. Such a pointer has no step,
 * and the array no size, which acc says when asked. */
typedef int list[];

list primes = { 2, 3, 5, 7 };           /* the initialiser gives its length */
extern int later[];
int (*to_later)[] = &later;
int later[3] = { 10, 20, 12 };
struct s *(*nothing)[];

static int sum(int (*p)[], int n)
{
    int s = 0;

    while (n--)
        s += (*p)[n];

    return s;
}

int main(void)
{
    int r = 0;
    int grid[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
    int (*row)[] = (int (*)[]) grid[1];

    if (sizeof primes / sizeof primes[0] == 4 && primes[3] == 7) r++;
    if ((*to_later)[2] == 12 && sum(to_later, 3) == 42) r++;
    if ((*row)[2] == 6 && !nothing) r++;

    return r + 39;              /* 3 checks */
}
