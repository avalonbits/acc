/* expect: 4: error: only an array's first dimension may be left out */
/* The rest are the size of a row, which every step through it needs. */
int main(void) {
    int m[2][] = {{1}, {2}};
    return 0;
}
