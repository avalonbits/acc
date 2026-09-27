/* expect: 3:16: error: an extern in a block cannot give a value */
int main(void) {
    extern int x = 5;
    return x;
}
