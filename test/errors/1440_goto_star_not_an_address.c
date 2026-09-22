/* expect: 3: error: 'goto *' needs an address, and this is an integer */
int main(void) {
    goto *3;
}
