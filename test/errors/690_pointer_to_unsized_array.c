/* expect: 4: error: a pointer to an array needs the array's size */
/* Stepping it goes a whole array at a time, which needs to know how far. */
int main(void) {
    int (*p)[];
    return 0;
}
