/* expect: 3: error: a compound literal needs a value */
int main(void) {
    int n = (int){ };
    return n;
}
