/* `&` in front of a compound literal with a subscript or a member after
 * it: the postfix binds to the literal first, so this is the address of an
 * element or a member. acc stopped at the literal and read the `[` as the
 * next thing in the statement. */
struct s { int a, b; };

int *g = &(int []){ 5, 6, 7 }[2];

int main(void)
{
    int a = 0, r = 0;
    int *p = &(int []){ 0, 1, 2 }[++a];
    int *q = &(struct s){ 3, 4 }.b;
    int v = (int []){ 9, 8 }[1];

    if (a == 1 && *p == 1) r++;
    if (*q == 4) r++;
    if (v == 8) r++;
    if (*g == 7) r++;

    return r + 38;              /* 4 checks */
}
