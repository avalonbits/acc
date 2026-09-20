/* expect: 5: error: an address is 3 bytes and this keeps only 1 of them */
int g;

int main(void) {
    char c = (char) (int) &g;

    return c;
}
