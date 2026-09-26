/* expect: 3:5: error: 'long double' is not supported: agondev's library has no arithmetic for one, so a program that asks for it does not link */
int main(void) {
    long double x = 1.5;
    return (int) x;
}
