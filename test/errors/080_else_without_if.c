/* expect: 4:5: error: 'else' without an 'if' */
int main(void) {
    int n = 1;
    else
        n = 2;
    return n;
}
