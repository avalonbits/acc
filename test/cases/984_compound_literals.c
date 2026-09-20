/* Compound literals: `(struct s){ 1, 2 }`, `(int[]){ 1, 2 }`, `(int){ 5 }`.
 * An unnamed object of the type in the parentheses, with a declaration's
 * initialiser -- so designators work in one, its address can be taken, and
 * at file scope it is static. */
struct point { int x, y; };
struct box { struct point at; int w; };

int *gp = (int[]){ 1, 2, 3 };
struct point *gq = &(struct point){ .y = 8 };
char *gs = (char[]){ 'h', 'i', 0 };

static int sum(struct point p) { return p.x * 10 + p.y; }

static int first(const int *p) { return p[0]; }

int main(void) {
    struct point p;
    struct point *q;
    int *a;

    /* As a value: assigned, passed, and its members read. */
    p = (struct point){ 3, 4 };
    if (p.x != 3 || p.y != 4)
        return 1;
    if (sum((struct point){ 5, 6 }) != 56)
        return 2;
    if ((struct point){ 7, 8 }.y != 8)
        return 3;
    if ((struct box){ { 1, 2 }, 3 }.at.y != 2)
        return 4;

    /* Designators inside one, and the members not named are zero. */
    if ((struct point){ .y = 9 }.y != 9 || (struct point){ .y = 9 }.x != 0)
        return 5;

    /* An array literal: subscripted, passed as a pointer, and with its
     * length taken from the values. */
    if ((int[]){ 4, 5, 6 }[1] != 5)
        return 6;
    a = (int[]){ 7, 8 };
    if (a[0] != 7 || a[1] != 8)
        return 7;
    if (first((const int[]){ 11, 12 }) != 11)
        return 8;
    if (sizeof((int[]){ 1, 2, 3 }) != 3 * sizeof(int))
        return 9;

    /* A scalar's, and the address of one. */
    if ((int){ 13 } != 13 || *&(int){ 14 } != 14)
        return 10;
    q = &(struct point){ 15, 16 };
    if (q->x != 15 || q->y != 16)
        return 11;

    /* It is an object: what it holds can be changed through it. */
    q->x = 17;
    if (q->x != 17)
        return 12;

    /* At file scope, where it is static. */
    if (gp[0] != 1 || gp[2] != 3)
        return 13;
    if (gq->y != 8 || gq->x != 0)
        return 14;
    if (gs[0] != 'h' || gs[1] != 'i' || gs[2] != 0)
        return 15;

    return 42;
}
