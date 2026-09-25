/* An interpreter: a small stack machine running a program of its own --
 * nested loops and arithmetic -- through a switch per instruction, which
 * is what an interpreter, an emulator or a parser spends its time in. */
#include "perf.h"

enum { PUSH, LOAD, STORE, ADD, SUB, MUL, AND, LT, JUMP, JZ, HALT };

static const unsigned char program[] = {
    PUSH, 0, STORE, 0,          /* total = 0 */
    PUSH, 0, STORE, 1,          /* i = 0 */
    /* 8: outer */
    LOAD, 1, LOAD, 3, LT, JZ, 57,
    PUSH, 0, STORE, 2,          /* j = 0 */
    /* 19: inner */
    LOAD, 2, PUSH, 40, LT, JZ, 48,
    LOAD, 0, LOAD, 1, LOAD, 2, MUL, ADD,
    PUSH, 255, AND, STORE, 0,   /* total = (total + i * j) & 255 */
    LOAD, 2, PUSH, 1, ADD, STORE, 2,
    JUMP, 19,
    /* 48: */
    LOAD, 1, PUSH, 1, ADD, STORE, 1,
    JUMP, 8,
    /* 57: */
    HALT
};

static int run(const unsigned char *code, int limit)
{
    int stack[16], vars[4] = { 0, 0, 0, 0 };
    int sp = 0, pc = 0;

    vars[3] = limit;
    for (;;) {
        switch (code[pc++]) {
        case PUSH:  stack[sp++] = code[pc++]; break;
        case LOAD:  stack[sp++] = vars[code[pc++]]; break;
        case STORE: vars[code[pc++]] = stack[--sp]; break;
        case ADD:   sp--; stack[sp - 1] += stack[sp]; break;
        case SUB:   sp--; stack[sp - 1] -= stack[sp]; break;
        case MUL:   sp--; stack[sp - 1] *= stack[sp]; break;
        case AND:   sp--; stack[sp - 1] &= stack[sp]; break;
        case LT:    sp--; stack[sp - 1] = stack[sp - 1] < stack[sp]; break;
        case JUMP:  pc = code[pc]; break;
        case JZ:    pc = stack[--sp] ? pc + 1 : code[pc]; break;
        case HALT:  return vars[0];
        default:    return -1;
        }
    }
}

int main(void)
{
    int limit = 30 + (int) (perf_seed & 1);
    unsigned long check;

    perf_start();
    check = (unsigned long) run(program, limit);
    check = check * 1000 + (unsigned long) run(program, limit - 7);
    perf_stop();

    perf_check(check);

    return 0;
}
