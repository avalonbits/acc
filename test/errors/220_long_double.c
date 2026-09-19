/* expect: 3: error: converting a float to a long double is not supported: agondev's library has no routine for it, and a program that asks for one does not link */
int main(void) {
    long double x = 1.5f;
    return (int) (long) x;
}
