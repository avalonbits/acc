/* int is 24 bits on this machine, not 32. 8388607 is the largest one, and
   adding to it wraps to the most negative. Prints 000029 -- 41. */
int main(void) {
    int big = 8388607;
    int wrapped = big + 1;
    return wrapped + 8388649;
}
