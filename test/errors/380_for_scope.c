/* expect: 9:12: error: 'i' is not declared */
/* A variable declared in a for loop's first clause ends with the loop.
 */
int main(void) {
    int n = 0;

    for (int i = 0; i < 3; i++)
        n += i;
    return i;
}
