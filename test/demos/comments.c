/* Comments, which acc has always taken and this file finally shows.
 *
 * Both kinds, anywhere a space can go. Prints 00002A -- 42. */
int add(int a, /* between parameters */ int b) {
    return a /* mid-expression */ + b;   // and to the end of the line
}

int main(void) {
    int x = 40;   // forty

    /* A comment spanning
       several lines, right before the thing that matters. */
    return add(x, 2);
}
