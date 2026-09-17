/* if, else, else if and while.
 *
 * A condition is just a value: true when it is not zero. There are no
 * comparison operators yet, so "n is zero" is written as `n` and "n is three"
 * as `n - 3`. Prints 00002A -- 42. */
int sum_to(int n) {
    int total = 0;

    while (n) {
        total = total + n;
        n = n - 1;
    }

    return total;
}

int grade(int a, int b) {
    if (a)
        return 1;
    else if (b)
        return 2;
    else
        return 3;
}

int main(void) {
    int r = sum_to(8);          /* 36 */

    if (r)
        r = r + grade(0, 1);    /* 2 */
    else
        r = 0;

    return r + 4;
}
