/* expect: 5:9: error: this is const, so it cannot be changed */
int x = 1;
const int *p = &x;
int main(void) {
    (*p)++;
    return x;
}
