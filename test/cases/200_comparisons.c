/* Every comparison, both ways round, as conditions and as values. */
int main(void) {
    int a = 5;
    int b = 9;
    int r = 0;

    if (a < b)  r = r + 1;
    if (b < a)  r = r + 100;
    if (b > a)  r = r + 2;
    if (a > b)  r = r + 100;
    if (a <= 5) r = r + 4;
    if (b <= 5) r = r + 100;
    if (a >= 5) r = r + 8;
    if (b <= a) r = r + 100;
    if (a == 5) r = r + 16;
    if (a == b) r = r + 100;
    if (a != b) r = r + 32;
    if (a != 5) r = r + 100;
    /* 1 + 2 + 4 + 8 + 16 + 32 = 63 */

    /* As values rather than conditions. */
    r = r + (a < b) + (b < a);
    r = r + (a == a) + (a == b);
    /* 63 + 1 + 0 + 1 + 0 = 65 */

    while (a < b)
        a = a + 1;
    /* a is 9 now */

    return r - a - 14;
}
