/* More values alive at once than the eZ80 has registers, so the compiler has
   to put some on the stack and fetch them back. Prints 00002F -- 47. */
int g(int a, int b) { return a - b; }

int main(void) {
    int a = 1, b = 2, c = 3, d = 4, e = 5;
    return ((a + b) - (c + d)) + ((e - a) + (b - c)) + (g(d, c) + g(e, b)) + 44;
}
