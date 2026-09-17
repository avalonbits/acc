/* expect: 3: error: 'signed' and 'unsigned' together */
int main(void) {
    signed unsigned int n = 1;
    return n;
}
