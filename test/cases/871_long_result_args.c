/* A long result of a function that takes arguments: its high byte comes
 * back in E, and taking the arguments off the stack must not overwrite it.
 * It did, and only a use of all four bytes showed it. */
long ident(const char *s, int x) {
    return x + s[0] - s[0];
}

long big(int a, int b, long c) {
    return c + a - b;
}

float scale(float f, int k) {
    return f * k;
}

int main(void) {
    int r = 0;

    if (ident("q", 10) == 10) r++;
    if (big(1, 1, 0x12345678) == 0x12345678) r++;
    if (big(0, 0, -2) < 0) r++;
    if (scale(1.5, 2) == 3.0) r++;

    return r + 38;          /* 4 checks */
}
