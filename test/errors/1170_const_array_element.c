/* expect: 4: error: this is const, so it cannot be changed */
const int table[3] = { 1, 2, 3 };
int main(void) {
    table[1] = 5;
    return 0;
}
