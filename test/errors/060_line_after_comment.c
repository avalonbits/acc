/* expect: 7:12: error: 'y' is not declared */
/* A block comment that spans
   several
   lines. The line counter has to keep up with them, or every diagnostic
   after a comment points at the wrong place. */
int main(void) {
    return y;
}
