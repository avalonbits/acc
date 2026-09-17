/* int is 24 bits here, so this wraps where a 32-bit int would not. The point
   of the test is that acc and agondev agree about where. */
int main(void) {
    int big = 8388607;          /* INT_MAX on this target */
    int wrapped = big + 1;      /* becomes INT_MIN */
    return wrapped + 8388650;
}
