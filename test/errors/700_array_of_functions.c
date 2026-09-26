/* expect: 4:21: error: an array of functions is not a thing C has; an array of pointers to them is */
/* Parentheses around a name are allowed, and change nothing: this is f[3] of functions. */
int main(void) {
    int (f[3])(void);
    return 0;
}
