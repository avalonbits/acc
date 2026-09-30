/* A call compiled in place, whose answer is a constant, as an argument of
 * another call. The first pass pops the answer into a register and pushes
 * it back -- and pushed it as HL whatever register it had popped it into:
 * with HL holding the argument before it, the answer went to DE, and HL
 * was passed twice. And opt-acc's SSA form, which keeps the constant, took
 * the record of that push whole, kind and all, so that 7 was passed on as
 * register 0: gcc's 20001130-1, where it recursed for ever. */
static inline int seven(void) { return 7; }
static inline int minus_two(void) { return -2; }

static int twice(int v) { return v + v; }
static int both(int a, int b) { return a * 10 + b; }

int main(void)
{
    int right = 0;

    right += twice(seven()) == 14;
    right += both(seven(), minus_two()) == 68;
    right += twice(twice(seven())) == 28;
    return right == 3 ? 42 : right;
}
