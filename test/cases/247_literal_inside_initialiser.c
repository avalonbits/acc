/* A compound literal inside another initialiser.
 *
 * A compound literal is an object of its own, initialised the way any
 * object of its kind is -- and acc keeps the state of the initialiser it is
 * reading in file-scope variables. So a literal inside an initialiser took
 * that state over and never gave it back.
 *
 * In a function, the outer initialiser went on storing its members into the
 * literal, and at its end zeroed the members it had already given, the map
 * of what it had given having been cleared for the literal's use. gcc's
 * 20030224-2 is `.magic = (jint16_t) {0x1985}`.
 *
 * At file scope the literal started the outer global's buffer again from
 * nothing, and put the outer global's pending addresses down at its own
 * place in the image. gcc's 20050929-1 is a global made of nested literals'
 * addresses, three deep.
 */
typedef struct { short v; } j16;
struct node { j16 magic; j16 nodetype; int totlen; };

struct A { int i; int j; };
struct B { struct A *a; struct A *b; };
struct C { struct B *c; struct A *d; };

/* File scope: the outer global's own bytes and addresses around the
 * literals' -- `before` and `after` are what went missing. */
struct D { int before; struct A *a; int after; };
struct D d = { 11, &(struct A) { 1, 2 }, 22 };
struct B q = { &(struct A) { 3, 4 }, &(struct A) { 5, 6 } };
struct C e = { &(struct B) { &(struct A) { 7, 8 }, &(struct A) { 9, 10 } },
               &(struct A) { 12, 13 } };

int main(void) {
    int r = 0;

    /* In a function: members after the literal, and the ones before it. */
    {
        struct node m = { .magic = (j16) {0x1985}, .nodetype = (j16) {0x2003},
                          .totlen = 7 };

        if (m.magic.v == 0x1985 && m.nodetype.v == 0x2003 && m.totlen == 7) r++;
    }
    {
        struct node m = { (j16) {0x1985}, (j16) {0x2003}, 7 };

        if (m.magic.v == 0x1985 && m.nodetype.v == 0x2003 && m.totlen == 7) r++;
    }
    {
        int a[3] = { 4, ((int[]) {5, 6})[1], 7 };

        if (a[0] == 4 && a[1] == 6 && a[2] == 7) r++;
    }

    /* At file scope. */
    if (d.before == 11 && d.a->i == 1 && d.a->j == 2 && d.after == 22) r++;
    if (q.a->i == 3 && q.a->j == 4 && q.b->i == 5 && q.b->j == 6) r++;
    if (e.c->a->i == 7 && e.c->a->j == 8 && e.c->b->i == 9 && e.c->b->j == 10) r++;
    if (e.d->i == 12 && e.d->j == 13) r++;

    return r + 35;              /* 7 checks */
}
