/* expect: 3:13: error: 'z' is not a suffix a constant can have */
int main(void) {
    int n = 10z;
    return n;
}
