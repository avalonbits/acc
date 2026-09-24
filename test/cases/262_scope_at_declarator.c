/* C99 6.2.1p7: a name is in scope from the end of its declarator, which is
 * before its initialiser. acc put it in scope after, so an initialiser that
 * used the name was refused or, where an outer one of that name was there,
 * given the outer one. */
struct node { int n; struct node *next; };

struct node ring[2] = { { 0, &ring[1] }, { 1, &ring[0] } };
struct node self = { 5, &self };
void *where = &where;

int main(void)
{
    int r = 0;
    int x = 7;

    {
        char x = sizeof x;          /* the char, not the outer int */

        if (x == 1) r++;
    }
    {
        struct node s = { 0, &s };
        int *p = (int *) sizeof *p;
        int a[3] = { sizeof a / sizeof a[0], 0, 0 };
        static struct node st = { 1, &st };

        if (s.next == &s) r++;
        if ((int) p == sizeof(int)) r++;
        if (a[0] == 3) r++;
        if (st.next == &st) r++;
    }
    if (ring[0].next == &ring[1] && ring[1].next == &ring[0]) r++;
    if (self.next == &self && where == &where) r++;
    if (x == 7) r++;

    return r + 34;              /* 8 checks */
}
