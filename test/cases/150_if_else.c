/* Both arms of an if, and an if with no else. */
int pick(int c, int a, int b) {
    if (c)
        return a;
    else
        return b;
}

int main(void) {
    int r = 0;

    if (1)
        r = r + 40;
    if (0)
        r = r + 100;

    return r + pick(1, 2, 9) - pick(0, 9, 0);
}
