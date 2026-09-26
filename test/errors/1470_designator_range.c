/* expect: 3:18: error: a designator names element 5 of an array of 4 */
int main(void) {
    int a[4] = { [5] = 1 };
    return a[0];
}
