/* File-scope variables: every type, with and without an initial value, read
 * and written from more than one function, stepped, compound-assigned, and
 * reached through a pointer.
 */
int counter;
int first = 5, other = -7;
char c = -3;
unsigned char uc = 200;
short s = -1000;
unsigned short us = 60000;
long big = 100000;
long neg = -2000000000;
unsigned long ul = 4000000000u;
long small_neg = -5;
float f = 2.5;
float g = -3;
float tiny = 1.5e-3;
int folded = 3 * 4 + 1;
int *where = &counter;
char *nothing = 0;
unsigned int mask = 0xfff000;

int bump(void) {
    counter++;
    return counter;
}

int add_to_counter(int n) {
    counter += n;
    return counter;
}

long twice_big(void) {
    return big * 2;
}

int main(void) {
    int r = 0;
    int *p;

    /* Zero until something writes it, and shared between functions. */
    if (counter == 0) r = r + 1;
    bump();
    bump();
    if (counter == 2) r = r + 1;
    if (add_to_counter(5) == 7) r = r + 1;

    /* The initial values, each at its own width and sign. */
    if (first + other == -2) r = r + 1;
    if (c == -3 && uc == 200) r = r + 1;
    if (s == -1000 && us == 60000) r = r + 1;
    if (big == 100000 && twice_big() == 200000) r = r + 1;
    if (neg == -2000000000 && small_neg == -5) r = r + 1;
    if (ul == 4000000000u) r = r + 1;
    if (f == 2.5 && g == -3.0 && tiny == 1.5e-3) r = r + 1;
    if (folded == 13 && mask == 0xfff000) r = r + 1;

    /* A pointer initialised with a global's address, and written through. */
    *where = 40;
    if (counter == 40) r = r + 1;
    if (nothing == 0) r = r + 1;
    p = &first;
    *p += 10;
    if (first == 15) r = r + 1;

    /* Stores that truncate, and arithmetic in the global's own width. */
    c = 300;
    uc = uc + 100;
    if (c == 44 && uc == 44) r = r + 1;
    s = s * 40;
    if (s == 25536) r = r + 1;

    /* Prefix and postfix, and compound assignment on every width. */
    if (counter++ == 40 && counter == 41) r = r + 1;
    if (--counter == 40) r = r + 1;
    big += 5;
    neg -= 1;
    f *= 2;
    if (big == 100005 && neg == -2000000001 && f == 5.0) r = r + 1;
    uc <<= 1;
    if (uc == 88) r = r + 1;

    /* 20 */
    return r + 22;
}
