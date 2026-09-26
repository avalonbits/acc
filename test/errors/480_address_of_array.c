/* expect: 6:13: error: an array cannot be assigned to as a whole */
/* A row of an array of arrays is an array, and C has no assignment for one. */
int main(void) {
    int m[2][3];
    int *p = m[1];
    m[0] = p;
    return 0;
}
