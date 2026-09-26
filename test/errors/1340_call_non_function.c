/* expect: 4:12: error: only a function or a pointer to one can be called */
int main(void) {
    int x = 3;
    return x(1);
}
