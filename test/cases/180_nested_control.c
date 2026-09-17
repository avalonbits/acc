/* Loops inside loops and ifs inside loops, so that the jumps of one are
   emitted between the jumps of another and every hole still gets the address
   it was meant to have.

   The inner if also has a dangling else, which C binds to the nearest if --
   here the `j + -1` one, so the 10 is added when j is 1 and not when j is
   truthy. */
int main(void) {
    int i = 4;
    int r = 0;
    int j = 0;

    while (i) {
        j = i;
        while (j) {
            if (j)
                if (j + -1)
                    r = r + 1;
                else
                    r = r + 10;
            j = j - 1;
        }
        i = i - 1;
    }
    /* i = 4,3,2,1, and each inner loop counts down from i.
       j > 1 on 3 + 2 + 1 + 0 = 6 iterations, j == 1 on 4.
       6 * 1 + 4 * 10 = 46 */

    return r - 4;
}
