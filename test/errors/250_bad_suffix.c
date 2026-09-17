/* expect: 3: error: 'z' is not a suffix a constant can have */
int main(void) {
    int n = 10z;
    return n;
}
