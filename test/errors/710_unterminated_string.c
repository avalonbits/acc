/* expect: 4:15: error: a string is not closed on the line it starts on */
/* A string ends on the line it starts on. */
int main(void) {
    char *s = "no end;
    return 0;
}
