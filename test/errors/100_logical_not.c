/* expect: 4: error: '!' is not supported yet */
int main(void) {
    int n = 1;
    if (!n)
        return 1;
    return 0;
}
