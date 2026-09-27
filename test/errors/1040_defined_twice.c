/* expect: 3:5: error: 'x' is defined twice */
int x = 1;
int x = 2;
int main(void) {
    return x;
}
