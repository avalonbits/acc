/* More live values than there are registers, so the allocator has to spill.
   Every bug found in milestone 0 was here. */
int g(int a, int b) { return a - b; }
int main(void) {
    int a = 1, b = 2, c = 3, d = 4, e = 5;
    return ((a + b) - (c + d)) + ((e - a) + (b - c)) + (g(d, c) + g(e, b)) + 39;
}
