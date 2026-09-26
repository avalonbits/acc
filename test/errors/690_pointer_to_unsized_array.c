/* expect: 5:6: error: an array of unknown size has no step to take */
/* Stepping it goes a whole array at a time, which needs to know how far. */
int main(void) {
    int (*p)[] = 0;
    p++;
    return 0;
}
