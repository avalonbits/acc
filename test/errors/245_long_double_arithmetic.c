/* expect: 4: error: arithmetic on a long double is not supported: agondev's library has no routine for it, and a program that asks for one does not link */
int main(void) {
    long double a = 1.5L, b = 2.5L;
    return (int) (long) (a + b);
}
