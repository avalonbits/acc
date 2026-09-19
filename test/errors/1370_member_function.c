/* expect: 2: error: a member cannot be a function; a pointer to one can */
struct s { int f(int); };
int main(void) {
    return 0;
}
