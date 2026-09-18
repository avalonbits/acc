/* Expressions that keep all three registers busy while one more is needed.
 *
 * Making room in a register once meant spilling whichever value had been in
 * a register longest. That could free only the register the caller had
 * refused -- and the compiler stopped with "no register after spilling" --
 * or it could spill the operand the caller had just forced into place, which
 * was then read from the register as if it had not moved. Now only the value
 * in the way moves, to a free register or to the frame.
 */
int pressure(char *pc, int *pi, unsigned char *pu, char c, short s) {
    return (*pc ^ 1) | ((c - *pi) & (s ^ *pu));
}

int deeper(int *a, int *b, int *c, int *d) {
    return ((*a - *b) ^ (*c + *d)) + ((*a | *c) - (*b & *d));
}

int main(void) {
    int r = 0;
    char pc = -7;
    int pi = -5;
    unsigned char pu = 200;
    int a = 12;
    int b = 5;
    int c = 30;
    int d = 7;

    if (pressure(&pc, &pi, &pu, -7, -3000) == -8) r = r + 1;
    if (deeper(&a, &b, &c, &d) == 59) r = r + 1;

    /* 2 */
    return r + 40;
}
