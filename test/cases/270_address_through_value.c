/* `&(p + 1)->m` and `&((&a[1])->m)`: the parenthesis holds a pointer, a
 * value, and the chain after it is what makes the object whose address is
 * taken. acc read only a name, a `*`, a string or another parenthesis after
 * `&(`, and refused the rest -- const-addr-expr-1 has it at file scope, as
 * an address constant. */
struct U { int a, id; } items[2];

int *g1 = &((items + 1)->id);
int *g2 = &((&items[1])->id);
int *g3 = &(1 ? items + 1 : items)->id;

int main(void)
{
    int r = 0;
    int *p1 = &(items + 1)->id;
    int *p2 = &((0, items + 1)->id);
    struct U *q = items;

    if (g1 == &items[1].id && g2 == g1 && g3 == g1) r++;
    if (p1 == &items[1].id && p2 == p1) r++;
    if (&(q + 1)->a == &items[1].a) r++;

    return r + 39;              /* 3 checks */
}
