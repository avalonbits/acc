/* Computed goto: `&&label` and `goto *p`.
 *
 * acc only: agondev's backend cannot compile a label's address at all --
 * "unable to legalize instruction: G_BLOCK_ADDR" -- so this case has no
 * reference answer and is held to coming out at 42 on its own.
 *
 * Not C -- it is gcc's, and it is here because it is what a threaded
 * interpreter is written with. The loop at the end of every opcode jumps
 * straight to the next one instead of going back round a switch, which on a
 * chip with no branch prediction is the whole of the saving: one indirect
 * jump against a bounds test, a jump table read and two more jumps.
 *
 * `&&label` is the address of a place inside a function, as a void *. acc
 * shortens a function's jumps once it has been compiled, and throws the
 * bytes between away, so every label in it moves; an address written down
 * before that has to move with it. It does, because it goes in the
 * relocation table and the shortening pass rewrites what every relocation
 * in the function holds. The `if` inside the first opcode below is there to
 * make that matter: it is a jump that does get shortened, and it sits
 * between the table and three of the labels the table holds.
 */
static int interp(const unsigned char *prog) {
    void *tab[4];
    int acc = 0, pc = 0;

    /* Forward: none of these labels has been reached yet. */
    tab[0] = &&op_add;
    tab[1] = &&op_dbl;
    tab[2] = &&op_neg;
    tab[3] = &&op_end;
    goto *tab[prog[pc++]];

op_add:
    acc += prog[pc++];
    if (acc > 1000) acc = 0;            /* a jump that would be shortened */
    goto *tab[prog[pc++]];
op_dbl:
    acc *= 2;
    goto *tab[prog[pc++]];
op_neg:
    acc = -acc;
    goto *tab[prog[pc++]];
op_end:
    return acc;
}

/* A label reached before its address is taken, which is a constant and needs
 * no hole. */
static int counts_backwards(void) {
    int n = 0;
    void *top;

again:
    n++;
    top = &&again;
    if (n < 4)
        goto *top;

    return n;
}

/* The address kept somewhere else and jumped to later, which is what says it
 * is an ordinary value: it survives being put in a struct and handed back. */
struct thread { void *next; int mark; };

static struct thread pick(void *a, void *b, int which) {
    struct thread t;

    t.next = which ? a : b;
    t.mark = which;

    return t;
}

static int through_a_struct(int which) {
    struct thread t = pick(&&yes, &&no, which);

    goto *t.next;

yes:
    return t.mark + 10;
no:
    return t.mark + 20;
}

int main(void) {
    static const unsigned char p[] = { 0, 5, 1, 2, 0, 1, 3 };
    int r = 0;

    /* 0+5 = 5, doubled = 10, negated = -10, and one more = -9. */
    if (interp(p) == -9) r++;
    if (counts_backwards() == 4) r++;
    if (through_a_struct(1) == 11) r++;
    if (through_a_struct(0) == 20) r++;

    /* The address is a void *, so it compares and assigns like one. */
    {
        void *a = &&here, *b;

    here:
        b = &&here;
        if (a == b && a != (void *) 0) r++;
    }

    return r + 37;              /* 5 checks */
}
