/* expect: 2: error: 'auto' is for a variable in a block, not at file scope */
auto int x;
int main(void) {
    return 0;
}
