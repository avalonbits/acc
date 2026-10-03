/* Calls with values live across them -- as many as the registers BC, DE
 * and IY hold, and more, which go to the frame -- their answers an int in
 * HL or a byte in A; a char parameter; six arguments, pushed last first;
 * calls as arguments of calls; a call in a loop whose counter lives
 * across it; recursion; and a call of a function answering nothing. As
 * opt-acc's machine-level backend makes them, pushing the pairs live
 * across a call around its arguments. */
static int sink;

int add3(int a, int b, int c) { return a + b + c; }
char next_char(char c) { return (char) (c + 1); }
int six(int a, int b, int c, int d, int e, int f)
{
    return a - b + c * 2 - d + e * 3 - f;
}
void touch(int n) { sink += n; }
int fib(int n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }
unsigned char low(int n) { return (unsigned char) n; }

int across(int x, int *p)
{
    int a = x + 1, b = p[1], c = p[2] - x, d = x * 3, e = p[0] + p[3];
    int r = add3(a, b, c);

    touch(d);
    r += add3(r, e, d);

    return r + a + b + c + d + e;
}

int nested(int x)
{
    return add3(add3(x, 1, 2), next_char((char) x), six(x, 2, 3, 4, 5, 6));
}

int looped(int n)
{
    int i, total = 0;

    for (i = 0; i < n; i++)
        total += add3(i, total, low(i + 256));

    return total;
}

int main(void)
{
    int data[4] = { 10, 20, 30, 40 };
    int r = 0;

    r += across(5, data) == 283;
    r += nested(7) == 34;
    r += next_char('a') == 'b';
    r += six(1, 2, 3, 4, 5, 6) == 10;
    r += looped(5) == 52;
    r += fib(10) == 55;
    r += sink == 15;

    return r == 7 ? 42 : r;
}
