/* expect: 7:25: error: 'hidden' is not declared */
/* A block's static lasts as long as the program, but its name is the block's. */
int f(void) {
    static int hidden;
    return hidden;
}
int main(void) { return hidden; }
