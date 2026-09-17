/* A function with more calls in it than the frame could once hold.
 *
 * Every call spills the registers that are live across it, and those slots
 * used to be taken from the frame and never given back -- so a function
 * with about a hundred calls put a slot past the -128 that (ix+d) reaches
 * and the compiler refused it. The scratch area is reused now: it is empty
 * at every statement boundary, because that is where the value stack is
 * empty, so each statement's spills sit in the same bytes as the last
 * one's.
 *
 * 300 calls here, in one function, with a local live across all of them.
 */
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

int main(void) {
    int total = 0;
    total = total + add(0, 0) - sub(0, 0);
    total = total + add(1, 1) - sub(1, 1);
    total = total + add(2, 2) - sub(2, 0);
    total = total + add(3, 0) - sub(3, 1);
    total = total + add(4, 1) - sub(4, 0);
    total = total + add(0, 2) - sub(5, 1);
    total = total + add(1, 0) - sub(6, 0);
    total = total + add(2, 1) - sub(0, 1);
    total = total + add(3, 2) - sub(1, 0);
    total = total + add(4, 0) - sub(2, 1);
    total = total + add(0, 1) - sub(3, 0);
    total = total + add(1, 2) - sub(4, 1);
    total = total + add(2, 0) - sub(5, 0);
    total = total + add(3, 1) - sub(6, 1);
    total = total + add(4, 2) - sub(0, 0);
    total = total + add(0, 0) - sub(1, 1);
    total = total + add(1, 1) - sub(2, 0);
    total = total + add(2, 2) - sub(3, 1);
    total = total + add(3, 0) - sub(4, 0);
    total = total + add(4, 1) - sub(5, 1);
    total = total + add(0, 2) - sub(6, 0);
    total = total + add(1, 0) - sub(0, 1);
    total = total + add(2, 1) - sub(1, 0);
    total = total + add(3, 2) - sub(2, 1);
    total = total + add(4, 0) - sub(3, 0);
    total = total + add(0, 1) - sub(4, 1);
    total = total + add(1, 2) - sub(5, 0);
    total = total + add(2, 0) - sub(6, 1);
    total = total + add(3, 1) - sub(0, 0);
    total = total + add(4, 2) - sub(1, 1);
    total = total + add(0, 0) - sub(2, 0);
    total = total + add(1, 1) - sub(3, 1);
    total = total + add(2, 2) - sub(4, 0);
    total = total + add(3, 0) - sub(5, 1);
    total = total + add(4, 1) - sub(6, 0);
    total = total + add(0, 2) - sub(0, 1);
    total = total + add(1, 0) - sub(1, 0);
    total = total + add(2, 1) - sub(2, 1);
    total = total + add(3, 2) - sub(3, 0);
    total = total + add(4, 0) - sub(4, 1);
    total = total + add(0, 1) - sub(5, 0);
    total = total + add(1, 2) - sub(6, 1);
    total = total + add(2, 0) - sub(0, 0);
    total = total + add(3, 1) - sub(1, 1);
    total = total + add(4, 2) - sub(2, 0);
    total = total + add(0, 0) - sub(3, 1);
    total = total + add(1, 1) - sub(4, 0);
    total = total + add(2, 2) - sub(5, 1);
    total = total + add(3, 0) - sub(6, 0);
    total = total + add(4, 1) - sub(0, 1);
    total = total + add(0, 2) - sub(1, 0);
    total = total + add(1, 0) - sub(2, 1);
    total = total + add(2, 1) - sub(3, 0);
    total = total + add(3, 2) - sub(4, 1);
    total = total + add(4, 0) - sub(5, 0);
    total = total + add(0, 1) - sub(6, 1);
    total = total + add(1, 2) - sub(0, 0);
    total = total + add(2, 0) - sub(1, 1);
    total = total + add(3, 1) - sub(2, 0);
    total = total + add(4, 2) - sub(3, 1);
    total = total + add(0, 0) - sub(4, 0);
    total = total + add(1, 1) - sub(5, 1);
    total = total + add(2, 2) - sub(6, 0);
    total = total + add(3, 0) - sub(0, 1);
    total = total + add(4, 1) - sub(1, 0);
    total = total + add(0, 2) - sub(2, 1);
    total = total + add(1, 0) - sub(3, 0);
    total = total + add(2, 1) - sub(4, 1);
    total = total + add(3, 2) - sub(5, 0);
    total = total + add(4, 0) - sub(6, 1);
    total = total + add(0, 1) - sub(0, 0);
    total = total + add(1, 2) - sub(1, 1);
    total = total + add(2, 0) - sub(2, 0);
    total = total + add(3, 1) - sub(3, 1);
    total = total + add(4, 2) - sub(4, 0);
    total = total + add(0, 0) - sub(5, 1);
    total = total + add(1, 1) - sub(6, 0);
    total = total + add(2, 2) - sub(0, 1);
    total = total + add(3, 0) - sub(1, 0);
    total = total + add(4, 1) - sub(2, 1);
    total = total + add(0, 2) - sub(3, 0);
    total = total + add(1, 0) - sub(4, 1);
    total = total + add(2, 1) - sub(5, 0);
    total = total + add(3, 2) - sub(6, 1);
    total = total + add(4, 0) - sub(0, 0);
    total = total + add(0, 1) - sub(1, 1);
    total = total + add(1, 2) - sub(2, 0);
    total = total + add(2, 0) - sub(3, 1);
    total = total + add(3, 1) - sub(4, 0);
    total = total + add(4, 2) - sub(5, 1);
    total = total + add(0, 0) - sub(6, 0);
    total = total + add(1, 1) - sub(0, 1);
    total = total + add(2, 2) - sub(1, 0);
    total = total + add(3, 0) - sub(2, 1);
    total = total + add(4, 1) - sub(3, 0);
    total = total + add(0, 2) - sub(4, 1);
    total = total + add(1, 0) - sub(5, 0);
    total = total + add(2, 1) - sub(6, 1);
    total = total + add(3, 2) - sub(0, 0);
    total = total + add(4, 0) - sub(1, 1);
    total = total + add(0, 1) - sub(2, 0);
    total = total + add(1, 2) - sub(3, 1);
    total = total + add(2, 0) - sub(4, 0);
    total = total + add(3, 1) - sub(5, 1);
    total = total + add(4, 2) - sub(6, 0);
    total = total + add(0, 0) - sub(0, 1);
    total = total + add(1, 1) - sub(1, 0);
    total = total + add(2, 2) - sub(2, 1);
    total = total + add(3, 0) - sub(3, 0);
    total = total + add(4, 1) - sub(4, 1);
    total = total + add(0, 2) - sub(5, 0);
    total = total + add(1, 0) - sub(6, 1);
    total = total + add(2, 1) - sub(0, 0);
    total = total + add(3, 2) - sub(1, 1);
    total = total + add(4, 0) - sub(2, 0);
    total = total + add(0, 1) - sub(3, 1);
    total = total + add(1, 2) - sub(4, 0);
    total = total + add(2, 0) - sub(5, 1);
    total = total + add(3, 1) - sub(6, 0);
    total = total + add(4, 2) - sub(0, 1);
    total = total + add(0, 0) - sub(1, 0);
    total = total + add(1, 1) - sub(2, 1);
    total = total + add(2, 2) - sub(3, 0);
    total = total + add(3, 0) - sub(4, 1);
    total = total + add(4, 1) - sub(5, 0);
    total = total + add(0, 2) - sub(6, 1);
    total = total + add(1, 0) - sub(0, 0);
    total = total + add(2, 1) - sub(1, 1);
    total = total + add(3, 2) - sub(2, 0);
    total = total + add(4, 0) - sub(3, 1);
    total = total + add(0, 1) - sub(4, 0);
    total = total + add(1, 2) - sub(5, 1);
    total = total + add(2, 0) - sub(6, 0);
    total = total + add(3, 1) - sub(0, 1);
    total = total + add(4, 2) - sub(1, 0);
    total = total + add(0, 0) - sub(2, 1);
    total = total + add(1, 1) - sub(3, 0);
    total = total + add(2, 2) - sub(4, 1);
    total = total + add(3, 0) - sub(5, 0);
    total = total + add(4, 1) - sub(6, 1);
    total = total + add(0, 2) - sub(0, 0);
    total = total + add(1, 0) - sub(1, 1);
    total = total + add(2, 1) - sub(2, 0);
    total = total + add(3, 2) - sub(3, 1);
    total = total + add(4, 0) - sub(4, 0);
    total = total + add(0, 1) - sub(5, 1);
    total = total + add(1, 2) - sub(6, 0);
    total = total + add(2, 0) - sub(0, 1);
    total = total + add(3, 1) - sub(1, 0);
    total = total + add(4, 2) - sub(2, 1);

    return total - 39;
}
