/* expect: 4: error: converting between floating-point and integer is not implemented yet */
int main(void) {
    float a = 1.5;
    int n = a;
    return n;
}
