/* expect: 4: error: converting a long double to a float is not supported: agondev's library has no routine for it, and a program that asks for one does not link */
int main(void) {
    long double a = 1.5L;
    float f = (float) a;
    return (int) f;
}
