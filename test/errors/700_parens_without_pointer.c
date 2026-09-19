/* expect: 4: error: acc takes parentheses in a declarator only around a pointer, as in (*p)[4] */
/* Pointers to functions, which parentheses also declare, are not here yet. */
int main(void) {
    int (p)[4];
    return 0;
}
