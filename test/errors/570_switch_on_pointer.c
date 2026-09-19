/* expect: 5: error: a switch needs an integer, and this is a pointer */
/* The value of a switch has to be an integer. */
int main(void) {
    int *p = 0;
    switch (p) {
    }
    return 0;
}
