/* expect: 4: error: '<<' takes integers, not floating-point values */
int main(void) {
    float a = 5.0;
    float c = a << 1;
    return 0;
}
