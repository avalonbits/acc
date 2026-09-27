/* expect: 3:14: error: a compound literal needs a value */
int main(void) {
    int n = (int){ };
    return n;
}
