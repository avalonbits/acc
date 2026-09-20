/* expect: 3: error: 'long double' is not supported: agondev's library has no arithmetic for one, so a program that asks for it does not link */
int main(void) {
    float f = 1.5L;
    return (int) f;
}
